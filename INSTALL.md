# AskTux — Build & Install

## Dependencies

| Package | Purpose |
|---|---|
| `gtkmm-4.0` | GUI framework |
| `libcurl` | HTTP client |
| `libsqlite3-dev` | Configuration storage |
| `libwebkitgtk-6.0-dev` | HTML rendering of markdown output |
| `libcmark-dev` | CommonMark markdown-to-HTML conversion |
| `meson` (≥ 0.60) | Build system |
| C++17 compiler (g++ or clang++) | |

### Installing dependencies (Debian / Ubuntu)

```bash
sudo apt install libgtkmm-4.0-dev libcurl4-openssl-dev nlohmann-json3-dev \
                 libsqlite3-dev libwebkitgtk-6.0-dev libcmark-dev \
                 meson pkg-config
```

### Installing dependencies (Fedora)

```bash
sudo dnf install gtkmm4-devel libcurl-devel nlohmann-json-devel \
                 sqlite-devel webkitgtk6.0-devel cmark-devel \
                 meson pkg-config
```

### Installing dependencies (Arch)

```bash
sudo pacman -S gtkmm4 curl nlohmann-json sqlite webkitgtk-6.0 cmark \
               meson pkg-config
```

## Build

```bash
cd asktux

meson setup build
meson compile -C build
```

Optionally enable tests:

```bash
meson setup build -Dbuild_tests=true
meson compile -C build
meson test -C build
```

## Install

```bash
meson install -C build
```

This installs the `asktux` binary, the `.desktop` file, the icon, the CSS
stylesheet, and the database schema file to the prefix chosen during setup
(default `/usr/local/`).  The app will appear in your desktop launcher after
you log out and back in, or you can run it from a terminal:

```bash
asktux
```

## Database

On first run, AskTux creates `~/.config/asktux/config.db` automatically
using the schema installed alongside the binary.  To pre-initialise the
database (e.g. during a package install script):

```bash
mkdir -p ~/.config/asktux
sqlite3 ~/.config/asktux/config.db < /usr/share/asktux/schema.sql
```

### Default providers

| ID | Provider | Base URL | Default model |
|----|----------|----------|---------------|
| 1 | Ollama | `http://localhost:11434` | `llama3.1:8b` |
| 2 | OpenAI | `https://api.openai.com/v1` | — |
| 3 | Groq | `https://api.groq.com/openai/v1` | — |
| 4 | DeepSeek | `https://api.deepseek.com/v1` | — |
| 5 | Google AI | `https://generativelanguage.googleapis.com/v1beta/openai` | — |
| 6 | GitHub Models | `https://models.inference.ai.azure.com` | — |

Provider 1 (Ollama) is local and needs no API key.  All others require
an API key, entered in the Settings dialog.

## Configuration

Configuration is stored in `~/.config/asktux/config.db` (SQLite).  You can
edit it from the **Settings** dialog inside the app, or directly with any
SQLite tool.

Active settings stored in the `settings` table:

| Key | Default | Description |
|-----|---------|-------------|
| `provider` | `1` (Ollama) | Currently selected provider ID |
| `system_prompt` | *(built-in template)* | Custom system prompt with `{distro}`, `{desktop}`, etc. |

## Debug mode

```bash
asktux --debug
```

Enables detailed timing and trace logging (token timings, request dumps,
Ollama internal breakdown).  Useful for diagnosing performance issues.
