#include "StreamParser.h"
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

        // Progress / download status (no "response" key)
        if (j.contains("status") && !j.contains("response")) {
            std::string status = j["status"];
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

        // Token response
        if (j.contains("response")) {
            std::string token = j["response"];
            if (!token.empty() && on_token)
                on_token(token);
        }
        if (j.contains("done") && j["done"] == true) {
            if (on_finish) on_finish();
        }
    } catch (const std::exception& e) {
        std::cerr << "[LinHelp] Ollama parse error: " << e.what() << std::endl;
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
            std::cerr << "[LinHelp] OpenAI parse error: " << e.what() << std::endl;
        }
    }
}
