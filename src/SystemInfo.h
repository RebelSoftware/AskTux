#ifndef LINHELP_SYSTEM_INFO_H
#define LINHELP_SYSTEM_INFO_H

#include <string>

/**
 * SystemInfo — collects read-only system context.
 *
 * All methods use Gio::Subprocess with a 2-second timeout.
 * On failure they return "Unknown".
 */
struct SystemInfo {
    std::string distro;           // e.g. "Ubuntu 24.04"
    std::string desktop;          // e.g. "GNOME"
    std::string desktop_version;  // e.g. "46"
    std::string window_system;    // "X11" or "Wayland"
    std::string shell;            // e.g. "/usr/bin/bash"
    std::string shell_version;    // e.g. "GNU bash, version 5.2.21(1)-release"
    std::string hardware;         // CPU model, RAM, GPU

    /** Gather everything once. */
    static SystemInfo gather();
};

#endif // LINHELP_SYSTEM_INFO_H
