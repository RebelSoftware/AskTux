#ifndef asktux_STREAM_PARSER_H
#define asktux_STREAM_PARSER_H

#include <string>
#include <functional>
#include <vector>

/**
 * StreamParser — parses chunks of streaming data from Ollama
 * (newline-delimited JSON) or OpenAI (Server-Sent Events).
 *
 * Usage:
 *   1. Create a StreamParser with the appropriate mode.
 *   2. Call feed(data) as each chunk arrives from libcurl.
 *   3. The on_token callback fires for every extracted token.
 *   4. The on_finish callback fires when a "done" signal is seen.
 */
class StreamParser {
public:
    enum class Mode { Ollama, OpenAI };

    StreamParser(Mode mode);

    /** Feed raw bytes from the HTTP stream. */
    void feed(const std::string& data);

    /** Reset internal state (e.g., before a new request). */
    void reset();

    // ── Callbacks ────────────────────────────────────────────────────────────
    std::function<void(const std::string&)> on_token;
    std::function<void(const std::string&)> on_progress;   // Ollama download status
    std::function<void(const std::string&)> on_error;      // Ollama/OpenAI error key
    std::function<void()>                   on_finish;

private:
    void parse_ollama_line(const std::string& line);
    void parse_openai_line(const std::string& line);

    Mode  mode_;
    std::string buffer_;   // for partial-line accumulation
};

#endif // asktux_STREAM_PARSER_H
