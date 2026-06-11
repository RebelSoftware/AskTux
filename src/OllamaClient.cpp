#include "OllamaClient.h"
#include "Config.h"

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
    curl_easy_setopt(curl, CURLOPT_USERAGENT,         "LinHelp/1.0");
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
    // PHASE 1 — Ensure model is available via /api/pull
    // ══════════════════════════════════════════════════════════════════════════
    {
        StreamParser pull_parser(StreamParser::Mode::Ollama);
        pull_parser.on_progress = wrap_progress;

        nlohmann::json pull_body;
        pull_body["model"]  = model;
        pull_body["stream"] = true;

        std::string pull_json = pull_body.dump();

        curl_easy_setopt(curl, CURLOPT_URL, (base_url + "/api/pull").c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    pull_json.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, pull_json.size());
        setup_curl(curl, headers, &pull_parser, &cancel_ctx);

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
    }

    // ══════════════════════════════════════════════════════════════════════════
    // PHASE 2 — Generate the response via /api/generate
    // ══════════════════════════════════════════════════════════════════════════
    {
        StreamParser gen_parser(StreamParser::Mode::Ollama);
        gen_parser.on_token    = std::move(on_token);
        gen_parser.on_progress = wrap_progress;  // same readable wrapper
        gen_parser.on_finish   = std::move(on_finish);

        nlohmann::json gen_body;
        gen_body["model"]   = model;
        gen_body["system"]  = system_prompt;
        gen_body["prompt"]  = user_question;
        gen_body["stream"]  = true;
        gen_body["options"] = {{"temperature", 0.2}};

        std::string gen_json = gen_body.dump();

        curl_easy_setopt(curl, CURLOPT_URL, (base_url + "/api/generate").c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    gen_json.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, gen_json.size());
        setup_curl(curl, headers, &gen_parser, &cancel_ctx);

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
