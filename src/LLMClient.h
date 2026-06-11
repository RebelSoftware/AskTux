#ifndef LINHELP_LLM_CLIENT_H
#define LINHELP_LLM_CLIENT_H

#include <string>
#include <functional>
#include <memory>
#include <vector>

/**
 * LLMClient — abstract base class for LLM backends.
 *
 * All communication runs on a background thread so the UI stays responsive.
 * Tokens are delivered one at a time via the on_token callback (called from
 * the background thread — the caller must marshal them to the main thread
 * via Glib::Dispatcher).
 */
class LLMClient {
public:
    using TokenCallback    = std::function<void(const std::string&)>;
    using ProgressCallback = std::function<void(const std::string&)>;
    using ErrorCallback    = std::function<void(const std::string&)>;
    using FinishCallback   = std::function<void()>;

    virtual ~LLMClient() = default;

    /**
     * Start a streaming request.
     *
     * @param system_prompt  The system prompt (placeholders expanded).
     * @param user_question  The user's question.
     * @param on_token       Called on the background thread for each token.
     * @param on_progress    Called on the background thread for download progress.
     * @param on_error       Called on the background thread on error.
     * @param on_finish      Called on the background thread when done.
     */
    virtual void send_request(
        const std::string& system_prompt,
        const std::string& user_question,
        TokenCallback    on_token,
        ProgressCallback on_progress,
        ErrorCallback    on_error,
        FinishCallback   on_finish
    ) = 0;

    /** Cancel an in-flight request. Thread-safe. */
    virtual void cancel() = 0;

    /** Factory: create the appropriate client based on config. */
    static std::unique_ptr<LLMClient> create();
};

#endif // LINHELP_LLM_CLIENT_H
