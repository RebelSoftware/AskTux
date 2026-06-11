// test_PromptBuilder.cpp
#include "../src/PromptBuilder.h"
#include <iostream>
#include <cassert>

int main()
{
    SystemInfo info;
    info.distro          = "Ubuntu 24.04";
    info.desktop         = "GNOME";
    info.desktop_version = "46";
    info.window_system   = "Wayland";
    info.shell           = "/usr/bin/bash";
    info.shell_version   = "GNU bash 5.2.21";

    std::string tmpl = "Distro: {distro}\nDesktop: {desktop} ({desktop_version})\n"
                       "WS: {window_system}\nShell: {shell}\n"
                       "Q: {user_question}";

    std::string result = PromptBuilder::build(tmpl, info, "How do I set the resolution?");

    std::cout << "--- Built prompt ---\n" << result << "\n--------------------\n";

    assert(result.find("Ubuntu 24.04") != std::string::npos);
    assert(result.find("GNOME (46)")   != std::string::npos);
    assert(result.find("Wayland")      != std::string::npos);
    assert(result.find("/usr/bin/bash") != std::string::npos);
    assert(result.find("How do I set the resolution?") != std::string::npos);
    assert(result.find("{distro}")     == std::string::npos);   // all replaced

    std::cout << "All PromptBuilder tests passed.\n";
    return 0;
}
