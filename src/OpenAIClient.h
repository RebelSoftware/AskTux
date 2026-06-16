#ifndef asktux_OPENAI_CLIENT_H
#define asktux_OPENAI_CLIENT_H

#include "LLMClient.h"
#include "StreamParser.h"

#include <string>
#include <thread>
#include <atomic>

/**
 * OpenAIClient — streams from an OpenAI-compatible API via /v1/chat/completions.
 *
 * Uses Server-Sent Events (SSE) streaming.
 */
class OpenAIClient : public LLMClient {
public:
    OpenAIClient();
    ~OpenAIClient() override;

    void send_request(
        const std::string& system_prompt,
        const std::string& user_question,
        TokenCallback    on_token,
        ProgressCallback on_progress,
        ErrorCallback    on_error,
        FinishCallback   on_finish
    ) override;

    void cancel() override { cancelled_ = true; }

private:
    void worker_thread(const std::string& url,
                       const std::string& json_body,
                       const std::string& api_key,
                       TokenCallback    on_token,
                       ProgressCallback on_progress,
                       ErrorCallback    on_error,
                       FinishCallback   on_finish);

    std::atomic<bool> cancelled_{false};
};

#endif // asktux_OPENAI_CLIENT_H
