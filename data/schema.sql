-- AskTux database schema
-- Applied automatically on first run; also usable by install scripts:
--   sqlite3 ~/.config/asktux/config.db < data/schema.sql

PRAGMA journal_mode=WAL;

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

CREATE TABLE IF NOT EXISTS models (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    provider_id INTEGER NOT NULL REFERENCES providers(id) ON DELETE CASCADE,
    name        TEXT NOT NULL,
    UNIQUE(provider_id, name)
);

-- ── Default settings (only inserted on a fresh database) ─────────────────────
INSERT OR IGNORE INTO settings (key, value) VALUES ('provider',      '1');
INSERT OR IGNORE INTO settings (key, value) VALUES ('system_prompt', 'You are AskTux, an expert Linux assistant. Your task is to give clear, safe, step‑by‑step instructions for using Linux.\n\nYou MUST:\n- Output only instructions — never suggest or generate commands that change system state unless the user explicitly confirms they want a "how to change" (even then, give manual steps, not auto‑execute).\n- Assume the user has the following system:\n  Distro: {distro}\n  Desktop: {desktop} ({desktop_version})\n  Display server: {window_system}\n  Shell: {shell}\n- Tailor every answer to this exact environment.\n- Prefer safe, reversible, and well‑documented methods.\n- Never hallucinate dangerous commands.\n- Assume the user is a beginner in Linux and if possible always suggest desktop based solutions and give well laid out instructions\n- If unsure, say "I don''t know based on your system info" and suggest general documentation.\n\n{tool_descriptions}\n\nUser question:\n{user_question}');

-- ── Default providers (common AI API endpoints) ──────────────────────────────
INSERT OR IGNORE INTO providers (id, name, base_url) VALUES (1, 'Ollama',     'http://localhost:11434');
INSERT OR IGNORE INTO providers (id, name, base_url) VALUES (2, 'OpenAI',    'https://api.openai.com/v1');
INSERT OR IGNORE INTO providers (id, name, base_url) VALUES (3, 'Groq',      'https://api.groq.com/openai/v1');
INSERT OR IGNORE INTO providers (id, name, base_url) VALUES (4, 'DeepSeek',  'https://api.deepseek.com/v1');
INSERT OR IGNORE INTO providers (id, name, base_url) VALUES (5, 'Google AI', 'https://generativelanguage.googleapis.com/v1beta');
INSERT OR IGNORE INTO providers (id, name, base_url) VALUES (6, 'GitHub Models', 'https://models.inference.ai.azure.com');

-- ── Default models per provider ──────────────────────────────────────────────
INSERT OR IGNORE INTO models (provider_id, name) VALUES (2, 'gpt-4o');
INSERT OR IGNORE INTO models (provider_id, name) VALUES (2, 'gpt-4o-mini');
INSERT OR IGNORE INTO models (provider_id, name) VALUES (2, 'gpt-4.1');
INSERT OR IGNORE INTO models (provider_id, name) VALUES (2, 'gpt-4.1-mini');
INSERT OR IGNORE INTO models (provider_id, name) VALUES (2, 'gpt-4.1-nano');
INSERT OR IGNORE INTO models (provider_id, name) VALUES (2, 'o3');
INSERT OR IGNORE INTO models (provider_id, name) VALUES (2, 'o4-mini');

INSERT OR IGNORE INTO models (provider_id, name) VALUES (3, 'llama-3.3-70b-versatile');
INSERT OR IGNORE INTO models (provider_id, name) VALUES (3, 'llama-4-scout-17b-16e-instruct');
INSERT OR IGNORE INTO models (provider_id, name) VALUES (3, 'mixtral-8x7b-32768');
INSERT OR IGNORE INTO models (provider_id, name) VALUES (3, 'deepseek-r1-distill-llama-70b');
INSERT OR IGNORE INTO models (provider_id, name) VALUES (3, 'gemma2-9b-it');

INSERT OR IGNORE INTO models (provider_id, name) VALUES (4, 'deepseek-chat');
INSERT OR IGNORE INTO models (provider_id, name) VALUES (4, 'deepseek-reasoner');

INSERT OR IGNORE INTO models (provider_id, name) VALUES (5, 'gemini-2.5-flash');
INSERT OR IGNORE INTO models (provider_id, name) VALUES (5, 'gemini-2.5-pro');

INSERT OR IGNORE INTO models (provider_id, name) VALUES (6, 'gpt-4o');
INSERT OR IGNORE INTO models (provider_id, name) VALUES (6, 'gpt-4o-mini');
INSERT OR IGNORE INTO models (provider_id, name) VALUES (6, 'DeepSeek-V3');
INSERT OR IGNORE INTO models (provider_id, name) VALUES (6, 'Phi-3.5-mini-instruct');
