# AskTux — LLM-Backed Linux Help Assistant

> ⚠️ **Prerelease — Not suitable for production use.**  
> This is an early-stage proof of concept. APIs, features, and behaviour will change without notice. Use at your own risk.

AskTux is a desktop application that lets you ask natural-language questions about using Linux and get step-by-step instructions from a local or remote LLM. It collects information about your system (distro, desktop environment, shell, hardware) and includes it as context so answers are tailored to your exact environment.

![Screenshot](https://img.shields.io/badge/status-pre--release-orange)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![GTK4](https://img.shields.io/badge/GTK-4-blue)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

---

## Features

- **Read-only assistance** — AskTux never executes commands. It provides instructions only.
- **Streaming responses** — LLM output streams token-by-token, rendered as formatted HTML in real time via WebKitGTK.
- **System-aware context** — Automatically detects your distro, desktop environment, display server, shell, and hardware to give relevant answers.
- **Local & remote backends** — Supports Ollama (local) and OpenAI-compatible APIs.
- **External styling** — Appearance is controlled by a CSS file that can be edited post-compilation.
- **Customisable system prompt** — Edit the prompt template in Settings to change AskTux's behaviour.

---

## Prerequisites

- **GTK 4** with **gtkmm 4.0** (C++ bindings)
- **libcurl**
- **nlohmann_json** (header-only JSON library)
- **WebKitGTK 6.0** (`libwebkitgtk-6.0-dev`) — for HTML rendering
- **libcmark** (`libcmark-dev`) — CommonMark markdown parser
- **Meson** — build system
- **sqlite** — For configuration and history
- A running **Ollama** instance (default) or an **OpenAI-compatible API** endpoint

### Quick install (Debian / Ubuntu)

```bash
sudo apt install meson libgtkmm-4.0-dev libcurl4-openssl-dev \
                 nlohmann-json3-dev libwebkitgtk-6.0-dev libcmark-dev
```

---

## Building

```bash
meson setup build
meson compile -C build
```

Run from the build directory:

```bash
./build/src/asktux
```

---

## Configuration

Settings are stored in `~/.config/asktux/config.json`. You can edit this file directly or use the **Settings** dialog inside the app.

| Setting | Default | Description |
|---------|---------|-------------|
| `backend` | `ollama` | `ollama` or `openai` |
| `model` | `llama3.2:3b` | LLM model name |
| `ollama_url` | `http://localhost:11434` | Ollama server URL |
| `openai_url` | `https://api.openai.com/v1` | OpenAI-compatible base URL |
| `openai_key` | *(empty)* | API key for remote backends |
| `system_prompt` | *(built-in template)* | Custom system prompt with `{distro}`, `{desktop}`, etc. |

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
