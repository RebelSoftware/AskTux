#include "Config.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <sys/stat.h>

// ── Replace literal "\n" (two chars) with actual newlines ────────────────────
static std::string unescape_newlines(std::string s)
{
    size_t pos = 0;
    while ((pos = s.find("\\n", pos)) != std::string::npos) {
        s.replace(pos, 2, "\n");
        ++pos;  // skip past the newly inserted newline
    }
    return s;
}


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

// ── Locate schema.sql ────────────────────────────────────────────────────────
/** Search for schema.sql using the same paths as the CSS file loader. */
static std::string find_schema_file()
{
    const char* prefix = std::getenv("ASKTUX_PREFIX");
    std::string base = prefix ? prefix : "/usr";

    // 1. Installed path.
    std::string installed = base + "/share/asktux/schema.sql";
    std::ifstream f(installed);
    if (f.good()) return installed;

    // 2. Development build paths.
    for (const auto& path : {"data/schema.sql", "../data/schema.sql",
                             "build/data/schema.sql"}) {
        f.open(path);
        if (f.good()) return path;
    }
    return "";
}

/** Read the full contents of a file into a string. */
static std::string read_file(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
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

    // Read schema.sql from the installed data directory.
    // No fallback — if the file is missing the install is broken and we
    // want to know immediately rather than silently drift out of sync.
    std::string schema_path = find_schema_file();
    std::string schema_sql  = read_file(schema_path);
    if (schema_sql.empty()) {
        std::cerr << "[AskTux] FATAL: Cannot find schema.sql (looked at: "
                  << (schema_path.empty() ? "(searched default paths)"
                                          : schema_path)
                  << ").  Reinstall AskTux or run the install script."
                  << std::endl;
        sqlite3_close(db_);
        db_ = nullptr;
        return;
    }

    char* err = nullptr;
    rc = sqlite3_exec(db_, schema_sql.c_str(), nullptr, nullptr, &err);
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

// ── Load ─────────────────────────────────────────────────────────────────────
void Config::load()
{
    ensure_db();
    if (!db_) return;

    // Read all settings into memory.
    {
        std::string pid_str = get_setting("provider", "1");
        provider_id_ = std::stoi(pid_str);
    }
    system_prompt_ = unescape_newlines(get_setting("system_prompt", default_system_prompt()));
}

// ── Save ─────────────────────────────────────────────────────────────────────
void Config::save()
{
    ensure_db();
    if (!db_) return;

    set_setting("provider",      std::to_string(provider_id_));
    set_setting("system_prompt", system_prompt_);
}

// ── Getters ──────────────────────────────────────────────────────────────────
int Config::provider_id() const { return provider_id_; }
std::string Config::system_prompt_template() const { return system_prompt_; }

std::string Config::model() const
{
    if (!db_) return "";
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT last_model FROM providers WHERE id = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return "";
    sqlite3_bind_int(stmt, 1, provider_id_);
    std::string model;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (val) model = val;
    }
    sqlite3_finalize(stmt);
    return model;
}

std::string Config::provider_name() const
{
    if (!db_) return "";
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT name FROM providers WHERE id = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return "";
    sqlite3_bind_int(stmt, 1, provider_id_);
    std::string name;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);
    return name;
}

std::string Config::provider_url() const
{
    if (!db_) return "";
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT base_url FROM providers WHERE id = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return "";
    sqlite3_bind_int(stmt, 1, provider_id_);
    std::string url;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);
    return url;
}

std::string Config::provider_api_key() const
{
    if (!db_) return "";
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT api_key FROM providers WHERE id = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return "";
    sqlite3_bind_int(stmt, 1, provider_id_);
    std::string key;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);
    return key;
}

// ── Setters ──────────────────────────────────────────────────────────────────
void Config::set_provider_id(int v)                 { provider_id_ = v; }
void Config::set_system_prompt_template(const std::string& v) { system_prompt_ = v; }

void Config::set_model(const std::string& v)
{
    if (!db_) return;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE providers SET last_model = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, v.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt,  2, provider_id_);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "[AskTux] Failed to save model: " << sqlite3_errmsg(db_)
                  << std::endl;
    }
    sqlite3_finalize(stmt);
}

// ── Validate ─────────────────────────────────────────────────────────────────
std::string Config::validate() const
{
    std::string cur_model = model();
    if (cur_model.empty())
        return "No model name configured. Open Settings and set a model.";

    if (!db_) return "Database not available.";

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT name, base_url, api_key FROM providers WHERE id = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return "Failed to look up provider.";
    sqlite3_bind_int(stmt, 1, provider_id_);

    std::string name, base_url, api_key;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        name     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        base_url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        api_key  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    }
    sqlite3_finalize(stmt);

    if (name.empty())
        return "Selected provider (ID " + std::to_string(provider_id_)
               + ") not found in database.";

    if (base_url.empty())
        return "Provider \"" + name + "\" has no URL configured.";

    // Ollama (id=1) doesn't need an API key; everyone else does.
    if (provider_id_ != 1 && api_key.empty())
        return "No API key configured for \"" + name
               + "\". Open Settings and add your API key.";

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
        "{tool_descriptions}\n\n"
        "User question:\n{user_question}";
}

// ── Providers ────────────────────────────────────────────────────────────────
std::vector<SavedProvider> Config::list_providers() const
{
    std::vector<SavedProvider> providers;
    if (!db_) return providers;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, name, base_url, api_key, last_model "
                      "FROM providers ORDER BY id;";
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

    // Prevent deleting the currently-selected provider.
    if (id == provider_id_) {
        std::cerr << "[AskTux] Cannot delete provider " << id
                  << " — it is currently selected. Switch to another provider first."
                  << std::endl;
        return;
    }

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

// ── Models ───────────────────────────────────────────────────────────────────
std::vector<SavedModel> Config::list_models(int provider_id) const
{
    std::vector<SavedModel> models;
    if (!db_) return models;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, provider_id, name FROM models "
                      "WHERE provider_id = ? ORDER BY name;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return models;
    sqlite3_bind_int(stmt, 1, provider_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SavedModel m;
        m.id          = sqlite3_column_int(stmt, 0);
        m.provider_id = sqlite3_column_int(stmt, 1);
        m.name        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        models.push_back(std::move(m));
    }
    sqlite3_finalize(stmt);
    return models;
}

void Config::save_model(int provider_id, const std::string& name)
{
    if (!db_) return;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT OR IGNORE INTO models (provider_id, name) "
                      "VALUES (?, ?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_int(stmt,  1, provider_id);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "[AskTux] Failed to save model: " << sqlite3_errmsg(db_)
                  << std::endl;
    }
    sqlite3_finalize(stmt);
}

void Config::delete_model(int id)
{
    if (!db_) return;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "DELETE FROM models WHERE id = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "[AskTux] Failed to delete model: " << sqlite3_errmsg(db_)
                  << std::endl;
    }
    sqlite3_finalize(stmt);
}
