#include "LLMClient.h"
#include "Config.h"
#include "OllamaClient.h"
#include "OpenAIClient.h"

std::unique_ptr<LLMClient> LLMClient::create()
{
    const auto& cfg = Config::instance();
    // Provider ID 1 = Ollama; everything else uses the OpenAI-compatible client.
    if (cfg.provider_id() == 1) {
        return std::make_unique<OllamaClient>();
    }
    return std::make_unique<OpenAIClient>();
}
