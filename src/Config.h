#ifndef LINHELP_CONFIG_H
#define LINHELP_CONFIG_H

#include <string>
#include <nlohmann/json.hpp>

/**
 * Config — singleton that loads and saves ~/.config/linhelp/config.json.
 *
 * Default values:
 *   backend       = "ollama"
 *   model         = "llama3.2:3b"
 *   ollama_url    = "http://localhost:11434"
 *   openai_url    = "https://api.openai.com/v1"
 *   openai_key    = ""
 *   system_prompt = <the default template from README.ai>
 */
class Config {
public:
    static Config& instance();

    // ── Load / Save ──────────────────────────────────────────────────────────
    void load();
    void save() const;

    // ── Getters ──────────────────────────────────────────────────────────────
    const std::string& backend()              const { return backend_; }
    const std::string& model()                const { return model_; }
    const std::string& ollama_url()           const { return ollama_url_; }
    const std::string& openai_url()           const { return openai_url_; }
    const std::string& openai_key()           const { return openai_key_; }
    const std::string& system_prompt_template() const { return system_prompt_; }

    // ── Setters ──────────────────────────────────────────────────────────────
    void set_backend(const std::string& v)              { backend_ = v; }
    void set_model(const std::string& v)                { model_ = v; }
    void set_ollama_url(const std::string& v)           { ollama_url_ = v; }
    void set_openai_url(const std::string& v)           { openai_url_ = v; }
    void set_openai_key(const std::string& v)           { openai_key_ = v; }
    void set_system_prompt_template(const std::string& v) { system_prompt_ = v; }

    /**
     * Validate the current configuration.
     * @return Empty string on success, or a human-readable error message.
     */
    std::string validate() const;

    static std::string default_system_prompt();

private:
    Config() = default;

    std::string backend_       = "ollama";
    std::string model_         = "llama3.2:3b";
    std::string ollama_url_    = "http://localhost:11434";
    std::string openai_url_    = "https://api.openai.com/v1";
    std::string openai_key_;
    std::string system_prompt_ = default_system_prompt();
};

#endif // LINHELP_CONFIG_H
