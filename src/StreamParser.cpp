#include "StreamParser.h"
#include "Log.h"

#include <nlohmann/json.hpp>
#include <iostream>

StreamParser::StreamParser(Mode mode) : mode_(mode) {}

void StreamParser::reset()
{
    buffer_.clear();
}

void StreamParser::feed(const std::string& data)
{
    buffer_ += data;

    switch (mode_) {
    case Mode::Ollama:
        // Ollama sends one JSON object per line.
        while (true) {
            auto pos = buffer_.find('\n');
            if (pos == std::string::npos) break;
            std::string line = buffer_.substr(0, pos);
            buffer_.erase(0, pos + 1);
            if (!line.empty())
                parse_ollama_line(line);
        }
        break;

    case Mode::OpenAI:
        // OpenAI uses SSE:  "data: {...}\n\n"
        while (true) {
            auto pos = buffer_.find("\n\n");
            if (pos == std::string::npos) break;
            std::string block = buffer_.substr(0, pos);
            buffer_.erase(0, pos + 2);
            parse_openai_line(block);
        }
        break;
    }
}

// ── Ollama: {"response":"token","done":false} or {"status":"pulling..."} ────
void StreamParser::parse_ollama_line(const std::string& line)
{
    try {
        auto j = nlohmann::json::parse(line);

        // Error response (no "response" or "status" key, just an error)
        if (j.contains("error") && !j.contains("response")) {
            std::string err = j["error"];
            std::cerr << "[AskTux] Ollama error in stream: " << err << std::endl;
            if (on_error) on_error(err);
            // Still check for "done" — some errors also have done:true.
            if (j.contains("done") && j["done"] == true && on_finish)
                on_finish();
            return;
        }

        // Progress / download status (no "response" key)
        if (j.contains("status") && !j.contains("response")) {
            std::string status = j["status"];
            // If the status itself signals an error, surface it.
            if (status == "error" && j.contains("error")) {
                std::string err = j["error"];
                std::cerr << "[AskTux] Ollama pull error: " << err << std::endl;
                if (on_error) on_error(err);
                return;
            }
            if (j.contains("completed") && j.contains("total")) {
                int64_t completed = j["completed"];
                int64_t total     = j["total"];
                if (total > 0) {
                    int pct = static_cast<int>(completed * 100 / total);
                    status += " (" + std::to_string(pct) + "%)";
                }
            }
            if (on_progress) on_progress(status);
            return;
        }

        // Token response — handle both /api/generate and /api/chat formats.
        if (j.contains("response")) {
            std::string token = j["response"];
            if (!token.empty()) {
                Log::dbg() << "[AskTux TRACE] token emit ('" << token << "')"
                           << std::endl;
                if (on_token) on_token(token);
            }
        } else if (j.contains("message") && j["message"].contains("content")) {
            std::string token = j["message"]["content"];
            if (!token.empty()) {
                Log::dbg() << "[AskTux TRACE] token emit ('" << token << "')"
                           << std::endl;
                if (on_token) on_token(token);
            }
        }
        if (j.contains("done") && j["done"] == true) {
            // Log Ollama's internal timing from the final message.
            auto ns_to_ms = [](int64_t ns) {
                return ns / 1'000'000;
            };
            int64_t total_dur   = j.value("total_duration", 0LL);
            int64_t load_dur    = j.value("load_duration", 0LL);
            int64_t prompt_dur  = j.value("prompt_eval_duration", 0LL);
            int64_t eval_dur    = j.value("eval_duration", 0LL);
            int64_t prompt_cnt  = j.value("prompt_eval_count", 0LL);
            int64_t eval_cnt    = j.value("eval_count", 0LL);

            Log::dbg() << "[AskTux TIMER] Ollama internal breakdown:" << std::endl;
            Log::dbg() << "[AskTux TIMER]   load_duration=" << ns_to_ms(load_dur)
                       << "ms  (model loading into VRAM)" << std::endl;
            Log::dbg() << "[AskTux TIMER]   prompt_eval_count=" << prompt_cnt
                       << "  prompt_eval_duration=" << ns_to_ms(prompt_dur)
                       << "ms  (" << (prompt_dur > 0 ? prompt_cnt * 1'000'000'000 / prompt_dur : 0)
                       << " tok/s)" << std::endl;
            Log::dbg() << "[AskTux TIMER]   eval_count=" << eval_cnt
                       << "  eval_duration=" << ns_to_ms(eval_dur)
                       << "ms  (" << (eval_dur > 0 ? eval_cnt * 1'000'000'000 / eval_dur : 0)
                       << " tok/s)" << std::endl;
            Log::dbg() << "[AskTux TIMER]   total_duration=" << ns_to_ms(total_dur)
                       << "ms" << std::endl;

            if (on_finish) on_finish();
        }
    } catch (const std::exception& e) {
        std::cerr << "[AskTux] Ollama parse error: " << e.what() << std::endl;
    }
}

// ── OpenAI SSE: "data: {\"choices\":[{\"delta\":{\"content\":\"token\"}}]}" ──
void StreamParser::parse_openai_line(const std::string& block)
{
    // Each line inside an SSE block starts with "data: ".
    std::istringstream stream(block);
    std::string line;
    while (std::getline(stream, line)) {
        // Remove trailing \r.
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.rfind("data: ", 0) != 0) continue;
        std::string payload = line.substr(6); // strip "data: "

        // "[DONE]" signal.
        if (payload == "[DONE]") {
            if (on_finish) on_finish();
            return;
        }

        try {
            auto j = nlohmann::json::parse(payload);
            auto& choices = j["choices"];
            if (choices.empty()) continue;

            auto& delta = choices[0]["delta"];
            if (delta.contains("content")) {
                std::string token = delta["content"];
                if (!token.empty() && on_token)
                    on_token(token);
            }

            if (choices[0].contains("finish_reason") &&
                !choices[0]["finish_reason"].is_null()) {
                if (on_finish) on_finish();
            }
        } catch (const std::exception& e) {
            std::cerr << "[AskTux] OpenAI parse error: " << e.what() << std::endl;
        }
    }
}
