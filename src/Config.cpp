#include "Config.h"

#include <fstream>
#include <iostream>
#include <cstdlib>
#include <sys/stat.h>
#include <nlohmann/json.hpp>

// ── Helpers ──────────────────────────────────────────────────────────────────
static std::string config_dir()
{
    const char* home = std::getenv("HOME");
    return (home ? std::string(home) : std::string("~")) + "/.config/asktux";
}

static std::string db_path()
{
    return config_dir() + "/config.db";
}

static std::string json_path()
{
    return config_dir() + "/config.json";
}

// ── Singleton ────────────────────────────────────────────────────────────────
Config& Config::instance()
{
    static Config cfg;
    return cfg;
}

Config::~Config()
{
    if (db_) sqlite3_close(db_);
}

// ── Database initialisation ──────────────────────────────────────────────────
void Config::ensure_db()
{
    if (db_) return;

    const auto dir = config_dir();
    mkdir(dir.c_str(), 0700);

    int rc = sqlite3_open(db_path().c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "[AskTux] Failed to open database: " << sqlite3_errmsg(db_)
                  << std::endl;
        return;
    }

    // Enable WAL mode for better concurrent read performance.
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS settings (
            key   TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS providers (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            name       TEXT NOT NULL UNIQUE,
            base_url   TEXT NOT NULL,
            api_key    TEXT DEFAULT '',
            last_model TEXT DEFAULT ''
        );
    )";

    char* err = nullptr;
    rc = sqlite3_exec(db_, schema, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "[AskTux] Failed to create tables: " << err << std::endl;
        sqlite3_free(err);
    }
}

// ── Key-value helpers ────────────────────────────────────────────────────────
std::string Config::get_setting(const std::string& key,
                                const std::string& default_val) const
{
    if (!db_) return default_val;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT value FROM settings WHERE key = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return default_val;
    }
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);

    std::string result = default_val;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (val) result = val;
    }
    sqlite3_finalize(stmt);
    return result;
}

void Config::set_setting(const std::string& key, const std::string& value)
{
    if (!db_) return;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"(
        INSERT INTO settings (key, value) VALUES (?, ?)
        ON CONFLICT(key) DO UPDATE SET value = excluded.value;
    )";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[AskTux] DB prepare error: " << sqlite3_errmsg(db_)
                  << std::endl;
        return;
    }
    sqlite3_bind_text(stmt, 1, key.c_str(),  -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "[AskTux] DB write error: " << sqlite3_errmsg(db_)
                  << std::endl;
    }
    sqlite3_finalize(stmt);
}

// ── Migrate from old config.json ─────────────────────────────────────────────
void Config::migrate_from_json()
{
    std::ifstream in(json_path());
    if (!in.is_open()) return;  // no JSON file to migrate

    std::cerr << "[AskTux] Migrating config.json to SQLite..." << std::endl;

    try {
        nlohmann::json j;
        in >> j;

        auto migrate = [&](const std::string& key, const std::string& def) {
            return j.contains(key) ? j[key].get<std::string>() : def;
        };

        set_setting("backend",       migrate("backend",       "ollama"));
        set_setting("model",         migrate("model",         "llama3.2:3b"));
        set_setting("ollama_url",    migrate("ollama_url",    "http://localhost:11434"));
        set_setting("openai_url",    migrate("openai_url",    "https://api.openai.com/v1"));
        set_setting("openai_key",    migrate("openai_key",    ""));
        set_setting("system_prompt", migrate("system_prompt", default_system_prompt()));

        // Rename old file so it isn't migrated twice.
        std::string backup = json_path() + ".migrated";
        std::rename(json_path().c_str(), backup.c_str());
        std::cerr << "[AskTux] Migrated from config.json (backup: config.json.migrated)"
                  << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[AskTux] Migration failed: " << e.what() << std::endl;
    }
}

// ── Load ─────────────────────────────────────────────────────────────────────
void Config::load()
{
    ensure_db();
    if (!db_) return;

    // Check for old config.json to migrate.
    migrate_from_json();

    // Read all settings into memory.
    backend_       = get_setting("backend",       "ollama");
    model_         = get_setting("model",         "llama3.2:3b");
    ollama_url_    = get_setting("ollama_url",    "http://localhost:11434");
    openai_url_    = get_setting("openai_url",    "https://api.openai.com/v1");
    openai_key_    = get_setting("openai_key",    "");
    system_prompt_ = get_setting("system_prompt", default_system_prompt());
}

// ── Save ─────────────────────────────────────────────────────────────────────
void Config::save()
{
    ensure_db();
    if (!db_) return;

    set_setting("backend",       backend_);
    set_setting("model",         model_);
    set_setting("ollama_url",    ollama_url_);
    set_setting("openai_url",    openai_url_);
    set_setting("openai_key",    openai_key_);
    set_setting("system_prompt", system_prompt_);
}

// ── Getters ──────────────────────────────────────────────────────────────────
std::string Config::backend()              const { return backend_; }
std::string Config::model()                const { return model_; }
std::string Config::ollama_url()           const { return ollama_url_; }
std::string Config::openai_url()           const { return openai_url_; }
std::string Config::openai_key()           const { return openai_key_; }
std::string Config::system_prompt_template() const { return system_prompt_; }

// ── Setters ──────────────────────────────────────────────────────────────────
void Config::set_backend(const std::string& v)              { backend_ = v; }
void Config::set_model(const std::string& v)                { model_ = v; }
void Config::set_ollama_url(const std::string& v)           { ollama_url_ = v; }
void Config::set_openai_url(const std::string& v)           { openai_url_ = v; }
void Config::set_openai_key(const std::string& v)           { openai_key_ = v; }
void Config::set_system_prompt_template(const std::string& v) { system_prompt_ = v; }

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

    return {};
}

// ── Default system prompt ────────────────────────────────────────────────────
std::string Config::default_system_prompt()
{
    return
        "You are AskTux, an expert Linux assistant. Your task is to give clear, "
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

// ── Providers ────────────────────────────────────────────────────────────────
std::vector<SavedProvider> Config::list_providers() const
{
    std::vector<SavedProvider> providers;
    if (!db_) return providers;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, name, base_url, api_key, last_model "
                      "FROM providers ORDER BY name;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return providers;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SavedProvider p;
        p.id         = sqlite3_column_int(stmt, 0);
        p.name       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        p.base_url   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        p.api_key    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        p.last_model = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        providers.push_back(std::move(p));
    }
    sqlite3_finalize(stmt);
    return providers;
}

void Config::save_provider(const SavedProvider& provider)
{
    if (!db_) return;

    sqlite3_stmt* stmt = nullptr;

    if (provider.id > 0) {
        // Update existing.
        const char* sql = "UPDATE providers SET name=?, base_url=?, api_key=?, "
                          "last_model=? WHERE id=?;";
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, provider.name.c_str(),      -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, provider.base_url.c_str(),  -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, provider.api_key.c_str(),   -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, provider.last_model.c_str(),-1, SQLITE_STATIC);
        sqlite3_bind_int(stmt,  5, provider.id);
    } else {
        // Insert new.
        const char* sql = "INSERT INTO providers (name, base_url, api_key, last_model) "
                          "VALUES (?, ?, ?, ?);";
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, provider.name.c_str(),      -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, provider.base_url.c_str(),  -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, provider.api_key.c_str(),   -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, provider.last_model.c_str(),-1, SQLITE_STATIC);
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "[AskTux] Failed to save provider: " << sqlite3_errmsg(db_)
                  << std::endl;
    }
    sqlite3_finalize(stmt);
}

void Config::delete_provider(int id)
{
    if (!db_) return;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "DELETE FROM providers WHERE id = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "[AskTux] Failed to delete provider: " << sqlite3_errmsg(db_)
                  << std::endl;
    }
    sqlite3_finalize(stmt);
}
