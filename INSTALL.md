# AskTux — Build & Install

## Dependencies

| Package | Purpose |
|---|---|
| `gtkmm-4.0` | GUI framework |
| `libcurl` | HTTP client |
| `nlohmann-json3-dev` | JSON parsing (header-only) |
| `meson` (≥ 0.60) | Build system |
| C++17 compiler (g++ or clang++) | |

### Installing dependencies (Debian / Ubuntu)

```bash
sudo apt install libgtkmm-4.0-dev libcurl4-openssl-dev nlohmann-json3-dev meson pkg-config
```

### Installing dependencies (Fedora)

```bash
sudo dnf install gtkmm4-devel libcurl-devel nlohmann-json-devel meson pkg-config
```

### Installing dependencies (Arch)

```bash
sudo pacman -S gtkmm4 curl nlohmann-json meson pkg-config
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

This installs the `asktux` binary, the `.desktop` file, and the icon to the
prefix chosen during setup (default `/usr/local/`).  The app will appear in your
desktop launcher after you log out and back in, or you can run it from a terminal:

```bash
asktux
```

## Configuration

Configuration is stored at `~/.config/asktux/config.json`.  You can also edit it
from the **Settings** dialog inside the app.

Default values:
- Backend: `ollama`
- Model: `llama3.2:3b`
- Ollama URL: `http://localhost:11434`
