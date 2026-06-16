#ifndef ASKTUX_CONFIG_H
#define ASKTUX_CONFIG_H

#include <string>
#include <vector>
#include <sqlite3.h>

/**
 * SavedProvider — a remembered OpenAI-compatible endpoint so the user can
 * quickly switch between providers without re-entering details.
 */
struct SavedProvider {
    int     id   = 0;          // database rowid, 0 = not persisted
    std::string name;          // user-friendly label (e.g. "OpenAI", "Groq")
    std::string base_url;      // e.g. "https://api.openai.com/v1"
    std::string api_key;       // stored in plaintext (local machine only)
    std::string last_model;    // last model selected for this provider
};

/**
 * Config — singleton backed by ~/.config/asktux/config.db (SQLite).
 *
 * Settings are stored as key-value pairs in a `settings` table, making it
 * simple to add new fields without schema changes.  An old `config.json`
 * is automatically migrated on first run.
 *
 * All getters return by value (not reference) because the data lives in
 * SQLite, not in memory fields.
 */
class Config {
public:
    static Config& instance();

    // ── Lifecycle ────────────────────────────────────────────────────────────
    /** Open (or create) the database and load settings. */
    void load();

    /** Persist all current settings to the database. */
    void save();

    // ── Getters ──────────────────────────────────────────────────────────────
    std::string backend()              const;
    std::string model()                const;
    std::string ollama_url()           const;
    std::string openai_url()           const;
    std::string openai_key()           const;
    std::string system_prompt_template() const;

    // ── Setters ──────────────────────────────────────────────────────────────
    void set_backend(const std::string& v);
    void set_model(const std::string& v);
    void set_ollama_url(const std::string& v);
    void set_openai_url(const std::string& v);
    void set_openai_key(const std::string& v);
    void set_system_prompt_template(const std::string& v);

    // ── Validation ───────────────────────────────────────────────────────────
    std::string validate() const;

    static std::string default_system_prompt();

    // ── Providers (saved OpenAI-compatible endpoints) ────────────────────────
    std::vector<SavedProvider> list_providers() const;
    void save_provider(const SavedProvider& provider);
    void delete_provider(int id);

private:
    Config()  = default;
    ~Config();
    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;

    /** Ensure the directory and tables exist. */
    void ensure_db();

    /** Migrate from ~/.config/asktux/config.json if present. */
    void migrate_from_json();

    /** Read a single setting by key (returns default if missing). */
    std::string get_setting(const std::string& key,
                            const std::string& default_val) const;

    /** Write a single setting. */
    void set_setting(const std::string& key, const std::string& value);

    sqlite3* db_ = nullptr;

    // ── In-memory dirty flags (written to DB on save()) ──────────────────────
    std::string backend_;
    std::string model_;
    std::string ollama_url_;
    std::string openai_url_;
    std::string openai_key_;
    std::string system_prompt_;
};

#endif // ASKTUX_CONFIG_H
