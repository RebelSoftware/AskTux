#include "OpenAIClient.h"
#include "Config.h"
#include "ScopedTimer.h"
#include "Log.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>

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

OpenAIClient::OpenAIClient()  = default;
OpenAIClient::~OpenAIClient() = default;

void OpenAIClient::send_request(
    const std::string& system_prompt,
    const std::string& user_question,
    TokenCallback    on_token,
    ProgressCallback on_progress,
    ErrorCallback    on_error,
    FinishCallback   on_finish)
{
    cancelled_ = false;

    const auto& cfg     = Config::instance();
    std::string url     = cfg.provider_url() + "/v1/chat/completions";
    std::string api_key = cfg.provider_api_key();
    std::string model   = cfg.model();

    nlohmann::json body;
    body["model"]       = model;
    body["stream"]      = true;
    body["temperature"] = 0.2;
    body["messages"]    = nlohmann::json::array({
        {{"role", "system"}, {"content", system_prompt}},
        {{"role", "user"},   {"content", user_question}}
    });

    std::string json_body = body.dump();

    Log::dbg() << "\n[AskTux] ──── Sent to OpenAI ────" << std::endl;
    Log::dbg() << "[AskTux] URL: " << url << std::endl;
    Log::dbg() << "[AskTux] Model: " << model << std::endl;
    Log::dbg() << "[AskTux] System prompt: " << system_prompt.size()
               << " chars, ~" << (system_prompt.size() / 4) << " tokens"
               << std::endl;
    Log::dbg() << "[AskTux] User question: " << user_question.size()
               << " chars" << std::endl;
    Log::dbg() << "[AskTux] Total request body: " << json_body.size()
               << " bytes" << std::endl;
    Log::dbg() << "[AskTux] ─────────────────────────\n" << std::endl;

    std::thread t(&OpenAIClient::worker_thread, this,
                  std::move(url), std::move(json_body),
                  std::move(api_key),
                  std::move(on_token), std::move(on_progress),
                  std::move(on_error), std::move(on_finish));
    t.detach();
}

void OpenAIClient::worker_thread(
    const std::string& url,
    const std::string& json_body,
    const std::string& api_key,
    TokenCallback    on_token,
    ProgressCallback on_progress,
    ErrorCallback    on_error,
    FinishCallback   on_finish)
{
    ScopedTimer timer("OpenAIClient::worker_thread total");

    StreamParser parser(StreamParser::Mode::OpenAI);

    // Shared state for TTFT (time to first token).
    struct TtftState {
        std::chrono::steady_clock::time_point before_request;
        std::atomic<bool> first_token_seen{false};
    };
    auto ttft_state = std::make_shared<TtftState>();

    parser.on_progress = std::move(on_progress);
    parser.on_finish   = std::move(on_finish);

    CancelCtx cancel_ctx{&cancelled_};

    CURL* curl = curl_easy_init();
    if (!curl) {
        if (on_error) on_error("Failed to initialise libcurl");
        return;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!api_key.empty()) {
        headers = curl_slist_append(headers,
            ("Authorization: Bearer " + api_key).c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL,              url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,        headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,        json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,     json_body.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,     write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,         &parser);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,    10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,           0L);  // no hard timeout (cancel button)
    curl_easy_setopt(curl, CURLOPT_USERAGENT,         "asktux/1.0");
    // Progress callback for cancel support.
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS,        0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION,  progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA,      &cancel_ctx);

    // Mark the start just before the blocking call.
    ttft_state->before_request = std::chrono::steady_clock::now();
    parser.on_token = [ttft_state, on_token = std::move(on_token)](const std::string& token) {
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
        std::string err = curl_easy_strerror(res);
        std::string hint;
        switch (res) {
        case CURLE_COULDNT_CONNECT:
            hint = "Could not connect to " + url +
                   ". Check the URL in Settings and verify your internet "
                   "connection.";
            break;
        case CURLE_OPERATION_TIMEDOUT:
            hint = "The API did not respond within 30 seconds. "
                   "Check your internet connection or try a different model "
                   "in Settings.";
            break;
        case CURLE_COULDNT_RESOLVE_HOST:
            hint = "Could not resolve host. Check the API base URL in Settings.";
            break;
        case CURLE_HTTP_RETURNED_ERROR: {
            // Try to extract HTTP status code for auth errors.
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code == 401 || http_code == 403)
                hint = "Authentication failed (HTTP " + std::to_string(http_code) +
                       "). Check your API key in Settings.";
            else
                hint = "HTTP error " + std::to_string(http_code) +
                       ". Check the API URL and model name in Settings.";
            break;
        }
        default:
            hint = "API request failed: " + err;
            break;
        }
        if (on_error) on_error(hint);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}
