# AskTux — LLM-Backed Linux Help Assistant

> ⚠️ **Prerelease — Not suitable for production use.**  
> This is an early-stage proof of concept. APIs, features, and behaviour will change without notice. Use at your own risk.

AskTux is a desktop application that lets you ask natural-language questions about using Linux and get step-by-step instructions from a local or remote LLM. It collects information about your system (distro, desktop environment, shell, hardware) and includes it as context so answers are tailored to your exact environment.

![Screenshot](https://img.shields.io/badge/status-pre--release-orange)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![GTK4](https://img.shields.io/badge/GTK-4-blue)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

---
## Who is Tux?
![Tux](https://upload.wikimedia.org/wikipedia/commons/3/35/Tux.svg){width=100px height=50px} Tux is a penguin character and the official mascot of the Linux kernel. 


## Features

- **Read-only assistance** — AskTux never executes commands. It provides instructions only.
- **Streaming responses** — LLM output streams token-by-token, rendered as formatted HTML in real time via WebKitGTK.
- **System-aware context** — Automatically detects your distro, desktop environment, display server, shell, and hardware to give relevant answers.
- **Local & remote providers** — Supports Ollama (local) and any OpenAI-compatible
  API (OpenAI, Groq, DeepSeek, Google AI, GitHub Models, etc.) with saved
  provider profiles and per-provider model lists.
- **External styling** — Appearance is controlled by a CSS file that can be edited post-compilation.
- **Customisable system prompt** — Edit the prompt template in Settings to change AskTux's behaviour.

---

## Prerequisites

- **GTK 4** with **gtkmm 4.0** (C++ bindings)
- **libcurl**
- **WebKitGTK 6.0** (`libwebkitgtk-6.0-dev`) — for HTML rendering
- **libcmark** (`libcmark-dev`) — CommonMark markdown parser
- **SQLite 3** (`libsqlite3-dev`) — configuration storage
- **Meson** — build system
- A running **Ollama** instance (default) or an **OpenAI-compatible API** endpoint

## Building

See [INSTALL.md](INSTALL.md) for full dependencies, build, and install
instructions.  Quick start:

```bash
sudo apt install meson libgtkmm-4.0-dev libcurl4-openssl-dev \
                 libwebkitgtk-6.0-dev libcmark-dev libsqlite3-dev

meson setup build
meson compile -C build
./build/src/asktux
```

---

## Configuration

Settings are stored in `~/.config/asktux/config.db` (SQLite). You can edit it
directly with any SQLite tool, or use the **Settings** dialog inside the app.

### Default providers

| ID | Provider | Base URL | Default model |
|----|----------|----------|---------------|
| 1 | Ollama | `http://localhost:11434` | `llama3.1:8b` |
| 2 | OpenAI | `https://api.openai.com/v1` | — |
| 3 | Groq | `https://api.groq.com/openai/v1` | — |
| 4 | DeepSeek | `https://api.deepseek.com/v1` | — |
| 5 | Google AI | `https://generativelanguage.googleapis.com/v1beta/openai` | — |
| 6 | GitHub Models | `https://models.inference.ai.azure.com` | — |

- Provider ID **1 (Ollama)** uses a local model and does not need an API key.
- All other providers require an API key (entered in Settings).
- Each provider remembers the last-used model (`last_model` column).
- The model dropdown is populated from saved models and (for Ollama) live
  queries to the running instance.

### Settings stored in the database

| Key | Default | Description |
|-----|---------|-------------|
| `provider` | `1` (Ollama) | Currently selected provider ID |
| `system_prompt` | *(built-in template)* | Custom system prompt with `{distro}`, `{desktop}`, etc. |

### Debug mode

```bash
./build/src/asktux --debug
```

Enables detailed timing and trace logging (ScopedTimer, request dumps, Ollama
internal breakdown). Useful for diagnosing performance issues.

---

## Styling

The markdown output is styled via `data/style.css`, installed to `/usr/share/asktux/style.css`. Edit this file to customise fonts, colours, and layout — no recompilation required.

---

## Roadmap

See [ROADMAP.ai](ROADMAP.ai) for planned improvements. Highlights include:

- Smarter model-pull logic (only pull if not already available)
- Chat history for follow-up questions
- Read-only tool use (local man pages, installed packages)
- OpenAPI-compatible endpoint support
- Distribution packages (.deb, .rpm, AppImage)

---

## License

MIT
