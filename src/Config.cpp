#include "Config.h"

#include <fstream>
#include <iostream>
#include <cstdlib>
#include <sys/stat.h>

// ── Helper: path to config file ──────────────────────────────────────────────
static std::string config_dir()
{
    const char* home = std::getenv("HOME");
    return (home ? std::string(home) : std::string("~")) + "/.config/linhelp";
}

static std::string config_path()
{
    return config_dir() + "/config.json";
}

// ── Singleton ────────────────────────────────────────────────────────────────
Config& Config::instance()
{
    static Config cfg;
    return cfg;
}

// ── Validate ─────────────────────────────────────────────────────────────────
std::string Config::validate() const
{
    if (model_.empty())
        return "No model name configured. Open Settings and set a model.";

    if (backend_ == "ollama") {
        if (ollama_url_.empty())
            return "Ollama URL is empty. Open Settings and set the Ollama URL "
                   "(default: http://localhost:11434).";
    } else if (backend_ == "openai") {
        if (openai_url_.empty())
            return "OpenAI URL is empty. Open Settings and set the API base URL.";
        if (openai_key_.empty())
            return "No API key configured for OpenAI backend. Open Settings "
                   "and add your API key.";
    } else {
        return "Unknown backend \"" + backend_ + "\". Open Settings and select "
               "either \"ollama\" or \"openai\".";
    }

    return {};  // all good
}

// ── Default system prompt template ───────────────────────────────────────────
std::string Config::default_system_prompt()
{
    return
        "You are LinHelp, an expert Linux assistant. Your task is to give clear, "
        "safe, step‑by‑step instructions for using Linux.\n\n"
        "You MUST:\n"
        "- Output only instructions — never suggest or generate commands that "
        "change system state unless the user explicitly confirms they want a "
        "\"how to change\" (even then, give manual steps, not auto‑execute).\n"
        "- Assume the user has the following system:\n"
        "  Distro: {distro}\n"
        "  Desktop: {desktop} ({desktop_version})\n"
        "  Display server: {window_system}\n"
        "  Shell: {shell}\n"
        "- Tailor every answer to this exact environment.\n"
        "- Prefer safe, reversible, and well‑documented methods.\n"
        "- Never hallucinate dangerous commands.\n"
        "- If unsure, say \"I don't know based on your system info\" and "
        "suggest general documentation.\n\n"
        "User question:\n{user_question}";
}

// ── Load ─────────────────────────────────────────────────────────────────────
void Config::load()
{
    std::ifstream in(config_path());
    if (!in.is_open()) {
        // File doesn't exist yet — stick with defaults.
        return;
    }

    try {
        nlohmann::json j;
        in >> j;

        if (j.contains("backend"))       backend_       = j["backend"];
        if (j.contains("model"))         model_         = j["model"];
        if (j.contains("ollama_url"))    ollama_url_    = j["ollama_url"];
        if (j.contains("openai_url"))    openai_url_    = j["openai_url"];
        if (j.contains("openai_key"))    openai_key_    = j["openai_key"];
        if (j.contains("system_prompt")) system_prompt_ = j["system_prompt"];
    } catch (const std::exception& e) {
        std::cerr << "[LinHelp] Failed to parse config: " << e.what() << std::endl;
    }
}

// ── Save ─────────────────────────────────────────────────────────────────────
void Config::save() const
{
    // Ensure directory exists.
    const auto dir = config_dir();
    mkdir(dir.c_str(), 0700);

    nlohmann::json j;
    j["backend"]       = backend_;
    j["model"]         = model_;
    j["ollama_url"]    = ollama_url_;
    j["openai_url"]    = openai_url_;
    j["openai_key"]    = openai_key_;
    j["system_prompt"] = system_prompt_;

    std::ofstream out(config_path());
    if (!out.is_open()) {
        std::cerr << "[LinHelp] Failed to write config to " << config_path() << std::endl;
        return;
    }
    out << j.dump(2) << std::endl;
}
