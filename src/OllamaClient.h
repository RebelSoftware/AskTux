#ifndef asktux_OLLAMA_CLIENT_H
#define asktux_OLLAMA_CLIENT_H

#include "LLMClient.h"
#include "StreamParser.h"

#include <string>
#include <thread>
#include <atomic>
#include <vector>

/**
 * OllamaClient — streams from a local Ollama instance via its HTTP API.
 *
 * POSTs to http://localhost:11434/api/generate with "stream": true.
 * Parses newline-delimited JSON chunks.
 */
class OllamaClient : public LLMClient {
public:
    OllamaClient();
    ~OllamaClient() override;

    void send_request(
        const std::string& system_prompt,
        const std::string& user_question,
        TokenCallback    on_token,
        ProgressCallback on_progress,
        ErrorCallback    on_error,
        FinishCallback   on_finish
    ) override;

    void cancel() override { cancelled_ = true; }

    /** Query the Ollama API for installed models. Returns model names. */
    static std::vector<std::string> list_models();

private:
    void worker_thread(const std::string& base_url,
                       const std::string& model,
                       const std::string& system_prompt,
                       const std::string& user_question,
                       TokenCallback    on_token,
                       ProgressCallback on_progress,
                       ErrorCallback    on_error,
                       FinishCallback   on_finish);

    std::atomic<bool> cancelled_{false};
};

#endif // asktux_OLLAMA_CLIENT_H
