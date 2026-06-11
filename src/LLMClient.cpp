#include "LLMClient.h"
#include "Config.h"
#include "OllamaClient.h"
#include "OpenAIClient.h"

std::unique_ptr<LLMClient> LLMClient::create()
{
    const auto& cfg = Config::instance();
    if (cfg.backend() == "openai") {
        return std::make_unique<OpenAIClient>();
    }
    // Default: Ollama.
    return std::make_unique<OllamaClient>();
}
