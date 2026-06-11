// test_SystemInfo.cpp
// Basic smoke tests for SystemInfo.
//
// Build & run:
//   meson setup build -Dbuild_tests=true
//   meson compile -C build
//   meson test -C build

#include "../src/SystemInfo.h"
#include <iostream>
#include <cassert>

int main()
{
    auto info = SystemInfo::gather();

    std::cout << "Distro:          " << info.distro          << "\n"
              << "Desktop:         " << info.desktop         << "\n"
              << "Desktop version: " << info.desktop_version << "\n"
              << "Window system:   " << info.window_system   << "\n"
              << "Shell:           " << info.shell           << "\n"
              << "Shell version:   " << info.shell_version   << "\n"
              << "Hardware:        " << info.hardware        << "\n";

    // At minimum, distro and window system should be non-empty.
    assert(!info.distro.empty());
    assert(!info.window_system.empty());
    assert(!info.shell.empty());

    std::cout << "\nAll SystemInfo tests passed.\n";
    return 0;
}
