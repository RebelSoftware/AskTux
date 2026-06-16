#ifndef ASKTUX_SYSTEM_INFO_H
#define ASKTUX_SYSTEM_INFO_H

#include <string>
#include <optional>
#include <mutex>

/**
 * SystemInfo — collects read-only system context.
 *
 * Call start_background_gather() early at startup (e.g. in main())
 * so the info is gathered in a background thread and is ready by
 * the time the user submits their first question.
 */
struct SystemInfo {
    std::string distro;           // e.g. "Ubuntu 24.04"
    std::string desktop;          // e.g. "GNOME"
    std::string desktop_version;  // e.g. "46"
    std::string window_system;    // "X11" or "Wayland"
    std::string shell;            // e.g. "/usr/bin/bash"
    std::string shell_version;    // e.g. "GNU bash, version 5.2.21(1)-release"
    std::string hardware;         // CPU model, RAM, GPU

    /** Gather everything synchronously (blocking). */
    static SystemInfo gather();

    /**
     * Return cached system info.  If the background thread hasn't
     * completed yet, falls back to a synchronous gather (one-time).
     */
    static const SystemInfo& get();

    /** Start gathering system info in a background thread. */
    static void start_background_gather();

private:
    static std::optional<SystemInfo> cache_;
    static std::mutex                mutex_;
};

#endif // ASKTUX_SYSTEM_INFO_H
