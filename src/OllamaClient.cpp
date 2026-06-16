#include "OllamaClient.h"
#include "Config.h"
#include "ScopedTimer.h"
#include "Log.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <cstring>
#include <iostream>
#include <sstream>

// ── libcurl write callback ───────────────────────────────────────────────────
static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* parser = static_cast<StreamParser*>(userdata);
    size_t total = size * nmemb;
    Log::dbg() << "[AskTux TRACE] write_callback: " << total << " bytes"
               << std::endl;
    parser->feed(std::string(ptr, total));
    return total;
}

// ── libcurl progress callback (abort on cancel) ──────────────────────────────
struct CancelCtx {
    std::atomic<bool>* cancelled;
};
static int progress_callback(void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    auto* ctx = static_cast<CancelCtx*>(clientp);
    return ctx->cancelled->load() ? 1 : 0;
}

// ── Helper: configure common curl options ────────────────────────────────────
static void setup_curl(CURL* curl, struct curl_slist* headers,
                       StreamParser* parser, CancelCtx* ctx)
{
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,        headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,     write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,         parser);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,    10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,           0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,         "AskTux/1.0");
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS,        0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION,  progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA,      ctx);
    curl_easy_setopt(curl, CURLOPT_POST,              1L);
}

OllamaClient::OllamaClient()  = default;
OllamaClient::~OllamaClient() = default;

void OllamaClient::send_request(
    const std::string& system_prompt,
    const std::string& user_question,
    TokenCallback    on_token,
    ProgressCallback on_progress,
    ErrorCallback    on_error,
    FinishCallback   on_finish)
{
    cancelled_ = false;

    const auto& cfg = Config::instance();
    std::string base_url = cfg.ollama_url();
    std::string model    = cfg.model();

    // Spawn worker thread — passes individual components so it can do pull + generate.
    std::thread t(&OllamaClient::worker_thread, this,
                  base_url, model,
                  system_prompt, user_question,
                  std::move(on_token), std::move(on_progress),
                  std::move(on_error), std::move(on_finish));
    t.detach();
}

void OllamaClient::worker_thread(
    const std::string& base_url,
    const std::string& model,
    const std::string& system_prompt,
    const std::string& user_question,
    TokenCallback    on_token,
    ProgressCallback on_progress,
    ErrorCallback    on_error,
    FinishCallback   on_finish)
{
    ScopedTimer timer("OllamaClient::worker_thread total");
    CancelCtx cancel_ctx{&cancelled_};

    CURL* curl = curl_easy_init();
    if (!curl) {
        if (on_error) on_error("Failed to initialise libcurl");
        return;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Wrap progress: replace cryptic layer digests with the model name.
    auto wrap_progress = [model, on_progress](const std::string& raw) {
        if (!on_progress) return;
        std::string display = raw;
        if (display.rfind("pulling ", 0) == 0) {
            // Extract the part after "pulling " before any " (xx%)".
            std::string rest = display.substr(8);
            auto pct_pos = rest.find(" (");
            std::string name_part = (pct_pos != std::string::npos)
                                        ? rest.substr(0, pct_pos) : rest;
            // "pulling manifest" is the only readable pulling status.
            // Everything else is a layer digest hash.
            if (name_part != "manifest") {
                display = model + " — downloading layer";
                if (pct_pos != std::string::npos)
                    display += rest.substr(pct_pos);
            }
        }
        on_progress(display);
    };

    // ══════════════════════════════════════════════════════════════════════════
    // PHASE 1 — Check if model is already available; pull if not
    // ══════════════════════════════════════════════════════════════════════════
    {
        ScopedTimer phase1_timer("Phase 1: tags check + pull");
        // Use a dedicated curl handle for the tags check — same pattern as
        // list_models() — to avoid state pollution from the main handle.
        bool model_available = false;
        {
            ScopedTimer tags_timer("  Tags check: GET /api/tags");
            std::string tags_url = base_url + "/api/tags";
            std::string tags_response;

            CURL* tags_curl = curl_easy_init();
            if (tags_curl) {
                curl_easy_setopt(tags_curl, CURLOPT_URL, tags_url.c_str());
                curl_easy_setopt(tags_curl, CURLOPT_WRITEFUNCTION,
                    +[](char* ptr, size_t size, size_t nmemb, void* udata) -> size_t {
                        auto* s = static_cast<std::string*>(udata);
                        size_t total = size * nmemb;
                        s->append(ptr, total);
                        return total;
                    });
                curl_easy_setopt(tags_curl, CURLOPT_WRITEDATA, &tags_response);
                curl_easy_setopt(tags_curl, CURLOPT_TIMEOUT, 5L);
                curl_easy_setopt(tags_curl, CURLOPT_CONNECTTIMEOUT, 3L);
                curl_easy_setopt(tags_curl, CURLOPT_USERAGENT, "AskTux/1.0");

                CURLcode res = curl_easy_perform(tags_curl);
                long http_code = 0;
                curl_easy_getinfo(tags_curl, CURLINFO_RESPONSE_CODE, &http_code);
                curl_easy_cleanup(tags_curl);

                std::cerr << "[AskTux] GET /api/tags → HTTP " << http_code
                          << " (" << curl_easy_strerror(res) << ")"
                          << ", body=" << tags_response.size() << " bytes"
                          << std::endl;

                if (res == CURLE_OK && http_code == 200 && !tags_response.empty()) {
                    std::cerr << "[AskTux] /api/tags raw: "
                              << tags_response.substr(0, 300) << std::endl;
                    try {
                        auto j = nlohmann::json::parse(tags_response);
                        if (j.contains("models")) {
                            std::cerr << "[AskTux] /api/tags has "
                                      << j["models"].size() << " model(s):";
                            for (const auto& m : j["models"]) {
                                std::string name = m.contains("name")
                                    ? m["name"].get<std::string>()
                                    : "(unnamed)";
                                std::cerr << " \"" << name << "\"";
                                if (name == model) {
                                    model_available = true;
                                    std::cerr << " ← MATCH";
                                }
                            }
                            std::cerr << std::endl;
                        } else {
                            std::cerr << "[AskTux] /api/tags: no \"models\" key"
                                      << std::endl;
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "[AskTux] JSON parse error: "
                                  << e.what() << std::endl;
                    }
                } else {
                    std::cerr << "[AskTux] /api/tags check failed — will pull"
                              << std::endl;
                }
            }
        }

        std::cerr << "[AskTux] Model \"" << model << "\" "
                  << (model_available ? "found locally" : "NOT found locally")
                  << std::endl;

        if (cancelled_.load()) {
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return;
        }

        if (!model_available) {
            ScopedTimer pull_timer("  Pull: POST /api/pull");
            std::cerr << "[AskTux] Proceeding with POST /api/pull for \""
                      << model << "\"" << std::endl;
            if (on_progress) on_progress(model + " — not found locally, pulling…");

            StreamParser pull_parser(StreamParser::Mode::Ollama);
            pull_parser.on_progress = wrap_progress;
            bool pull_error = false;
            pull_parser.on_error = [&pull_error, model, on_error](const std::string& err) {
                pull_error = true;
                std::string msg = "Failed to pull \"" + model + "\": " + err;
                std::cerr << "[AskTux] " << msg << std::endl;
                if (on_error) on_error(msg);
            };

            nlohmann::json pull_body;
            pull_body["model"]      = model;
            pull_body["stream"]     = true;
            pull_body["keep_alive"] = "20m";

            std::string pull_json = pull_body.dump();

            // Restore POST / streaming config for /api/pull
            curl_easy_setopt(curl, CURLOPT_URL, (base_url + "/api/pull").c_str());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 0L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, pull_json.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, pull_json.size());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &pull_parser);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

            CURLcode res = curl_easy_perform(curl);

            if (cancelled_.load()) {
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                return;
            }
            if (res != CURLE_OK) {
                std::string hint;
                switch (res) {
                case CURLE_COULDNT_CONNECT:
                    hint = "Could not connect to Ollama at " + base_url +
                           ". Is Ollama running? (Try: systemctl --user start ollama)";
                    break;
                case CURLE_OPERATION_TIMEDOUT:
                    hint = "Ollama pull timed out. Check your network or the model name.";
                    break;
                case CURLE_COULDNT_RESOLVE_HOST:
                    hint = "Could not resolve host. Check the Ollama URL in Settings.";
                    break;
                default:
                    hint = "Ollama pull failed: " + std::string(curl_easy_strerror(res));
                    break;
                }
                if (on_error) on_error(hint);
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                return;
            }
            if (pull_error) {
                std::cerr << "[AskTux] Pull failed — aborting request" << std::endl;
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                return;
            }
            std::cerr << "[AskTux] POST /api/pull completed OK" << std::endl;
        } else {
            std::cerr << "[AskTux] Skipping /api/pull — model already available"
                      << std::endl;
        }
        phase1_timer.lap("Phase 1 complete");
    }

    // ══════════════════════════════════════════════════════════════════════════
    // PHASE 2 — Generate the response via /api/generate
    // ══════════════════════════════════════════════════════════════════════════
    {
        ScopedTimer gen_timer("Phase 2: POST /api/generate (until stream ends)");
        StreamParser gen_parser(StreamParser::Mode::Ollama);

        // Shared state for TTFT — use a heap-allocated struct so the
        // callback can access it reliably across move/copy.
        struct TtftState {
            std::chrono::steady_clock::time_point before_request;
            std::atomic<bool> first_token_seen{false};
        };
        auto ttft_state = std::make_shared<TtftState>();

        gen_parser.on_progress = wrap_progress;  // same readable wrapper
        gen_parser.on_error    = on_error;
        gen_parser.on_finish   = std::move(on_finish);

        nlohmann::json gen_body;
        gen_body["model"]      = model;
        gen_body["messages"]   = nlohmann::json::array({
            {{"role", "system"},    {"content", system_prompt}},
            {{"role", "user"},      {"content", user_question}}
        });
        gen_body["stream"]     = true;
        gen_body["keep_alive"] = "20m";  // keep model in VRAM between requests
        gen_body["options"]    = {{"temperature", 0.2}};

        std::string gen_json = gen_body.dump();

        // Log what's being sent to the LLM.
        Log::dbg() << "\n[AskTux] ──── Sent to LLM ────" << std::endl;
        Log::dbg() << "[AskTux] System prompt (" << system_prompt.size()
                   << " chars, ~" << (system_prompt.size() / 4) << " tokens):"
                   << std::endl;
        Log::dbg() << "[AskTux] >>>" << std::endl;
        Log::dbg() << system_prompt << std::endl;
        Log::dbg() << "[AskTux] <<<" << std::endl;
        Log::dbg() << "[AskTux] User question (" << user_question.size()
                   << " chars): " << user_question << std::endl;
        Log::dbg() << "[AskTux] Total request body: " << gen_json.size()
                   << " bytes" << std::endl;
        Log::dbg() << "[AskTux] ──────────────────────\n" << std::endl;

        curl_easy_setopt(curl, CURLOPT_URL, (base_url + "/api/chat").c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    gen_json.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, gen_json.size());
        setup_curl(curl, headers, &gen_parser, &cancel_ctx);

        // Mark the start just before the blocking call.
        ttft_state->before_request = std::chrono::steady_clock::now();
        gen_parser.on_token = [ttft_state, on_token](const std::string& token) {
            if (!ttft_state->first_token_seen.exchange(true)) {
                auto ttft = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now()
                                - ttft_state->before_request).count();
                Log::dbg() << "[AskTux TIMER]   TTFT (first token): " << ttft << " ms"
                           << std::endl;
            }
            if (on_token) on_token(token);
        };

        CURLcode res = curl_easy_perform(curl);

        if (cancelled_.load()) {
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return;
        }
        if (res != CURLE_OK) {
            std::string hint;
            switch (res) {
            case CURLE_COULDNT_CONNECT:
                hint = "Lost connection to Ollama during generation.";
                break;
            case CURLE_OPERATION_TIMEDOUT:
                hint = "Ollama generate timed out.";
                break;
            default:
                hint = "Ollama generate failed: " + std::string(curl_easy_strerror(res));
                break;
            }
            if (on_error) on_error(hint);
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

// ── List installed models via GET /api/tags ──────────────────────────────────
std::vector<std::string> OllamaClient::list_models()
{
    std::vector<std::string> models;
    const auto& cfg = Config::instance();
    std::string url = cfg.ollama_url() + "/api/tags";

    CURL* curl = curl_easy_init();
    if (!curl) return models;

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char* ptr, size_t size, size_t nmemb, void* udata) {
        auto* s = static_cast<std::string*>(udata);
        size_t total = size * nmemb;
        s->append(ptr, total);
        return total;
    });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        try {
            auto j = nlohmann::json::parse(response);
            if (j.contains("models")) {
                for (const auto& m : j["models"]) {
                    if (m.contains("name"))
                        models.push_back(m["name"]);
                }
            }
        } catch (...) { /* ignore parse errors */ }
    }
    curl_easy_cleanup(curl);
    return models;
}
