#include "SystemInfo.h"

#include <fstream>
#include <sstream>
#include <regex>
#include <array>
#include <cstdio>
#include <memory>

// ── Helper: run a command via popen with a 2-second timeout ──────────────────
static std::string run_command(const std::string& cmd)
{
    std::string result;
    std::array<char, 4096> buf;
    // Use timeout(1) to cap execution at 2 seconds.
    FILE* pipe = popen(("timeout 2 " + cmd + " 2>/dev/null").c_str(), "r");
    if (!pipe) return "Unknown";
    while (fgets(buf.data(), buf.size(), pipe) != nullptr) {
        result += buf.data();
    }
    pclose(pipe);
    // Trim trailing whitespace.
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result.empty() ? "Unknown" : result;
}

// ── Distro ───────────────────────────────────────────────────────────────────
static std::string detect_distro()
{
    std::ifstream in("/etc/os-release");
    if (!in.is_open()) return "Unknown";

    std::string name, version;
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("NAME=", 0) == 0) {
            name = line.substr(5);
            if (!name.empty() && name.front() == '"' && name.back() == '"')
                name = name.substr(1, name.size() - 2);
        } else if (line.rfind("VERSION_ID=", 0) == 0) {
            version = line.substr(11);
            if (!version.empty() && version.front() == '"' && version.back() == '"')
                version = version.substr(1, version.size() - 2);
        }
    }
    if (name.empty()) return "Unknown";
    if (version.empty()) return name;
    return name + " " + version;
}

// ── Desktop Environment ──────────────────────────────────────────────────────
static std::string detect_desktop()
{
    const char* de = std::getenv("XDG_CURRENT_DESKTOP");
    if (de) return std::string(de);

    const char* session = std::getenv("DESKTOP_SESSION");
    if (session) return std::string(session);

    // Fallback: check running processes.
    auto out = run_command("pgrep -l gnome-shell plasmashell xfce4-session sway hyprland 2>/dev/null | head -1");
    if (out != "Unknown") {
        auto pos = out.rfind(' ');
        if (pos != std::string::npos)
            return out.substr(pos + 1);
    }
    return "Unknown";
}

static std::string detect_desktop_version(const std::string& desktop)
{
    std::string de_lower;
    for (auto c : desktop) de_lower += static_cast<char>(std::tolower(c));

    if (de_lower.find("gnome") != std::string::npos)
        return run_command("gnome-shell --version 2>/dev/null | grep -oP '\\d+\\.\\d+'");
    if (de_lower.find("kde") != std::string::npos || de_lower.find("plasma") != std::string::npos)
        return run_command("plasmashell --version 2>/dev/null | grep -oP '\\d+\\.\\d+'");
    if (de_lower.find("xfce") != std::string::npos)
        return run_command("xfce4-session --version 2>/dev/null | grep -oP '\\d+\\.\\d+'");

    return "Unknown";
}

// ── Window System ────────────────────────────────────────────────────────────
static std::string detect_window_system()
{
    const char* ws = std::getenv("XDG_SESSION_TYPE");
    return ws ? std::string(ws) : "Unknown";
}

// ── Shell ────────────────────────────────────────────────────────────────────
static std::string detect_shell()
{
    const char* s = std::getenv("SHELL");
    return s ? std::string(s) : "Unknown";
}

static std::string detect_shell_version(const std::string& shell_path)
{
    if (shell_path == "Unknown") return "Unknown";
    return run_command(shell_path + " --version 2>/dev/null | head -1");
}

// ── Hardware Summary ─────────────────────────────────────────────────────────
static std::string detect_hardware()
{
    std::ostringstream out;

    // CPU model
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.rfind("model name", 0) == 0) {
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                out << "CPU: " << line.substr(colon + 2);
                // Only first CPU line.
                break;
            }
        }
    }

    // RAM
    std::ifstream meminfo("/proc/meminfo");
    while (std::getline(meminfo, line)) {
        if (line.rfind("MemTotal", 0) == 0) {
            out << " | RAM: " << line.substr(line.find(':') + 2);
            break;
        }
    }

    // GPU — try lspci first, fallback to /sys/class/drm
    auto gpu = run_command("lspci 2>/dev/null | grep -i 'vga\\|3d' | head -1 | cut -d':' -f3-");
    if (gpu != "Unknown" && !gpu.empty())
        out << " | GPU:" << gpu;

    return out.str();
}

// ── Gather ───────────────────────────────────────────────────────────────────
SystemInfo SystemInfo::gather()
{
    SystemInfo info;
    info.distro          = detect_distro();
    info.desktop         = detect_desktop();
    info.desktop_version = detect_desktop_version(info.desktop);
    info.window_system   = detect_window_system();
    info.shell           = detect_shell();
    info.shell_version   = detect_shell_version(info.shell);
    info.hardware        = detect_hardware();
    return info;
}
