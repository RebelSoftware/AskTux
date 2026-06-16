#include "MainWindow.h"
#include "Config.h"
#include "SystemInfo.h"
#include "Log.h"
#include "ScopedTimer.h"

#include <gtkmm.h>
#include <iostream>
#include <curl/curl.h>
#include <cstring>

int main(int argc, char* argv[])
{
    // Initialise libcurl globally (once per process).
    curl_global_init(CURL_GLOBAL_ALL);

    // Load saved configuration.
    Config::instance().load();

    // Check for --debug flag before GTK processes the args.
    // Remove it from argv so GTK doesn't complain about unknown options.
    {
        int write_idx = 1;
        for (int i = 1; i < argc; ++i) {
            if (std::strcmp(argv[i], "--debug") == 0) {
                Log::set_debug(true);
                ScopedTimer::enabled.store(true);
                std::cerr << "[AskTux] Debug logging enabled" << std::endl;
            } else {
                argv[write_idx++] = argv[i];
            }
        }
        if (write_idx < argc) argv[write_idx] = nullptr;
        argc = write_idx;
    }

    // Start gathering system info in the background so it's ready
    // by the time the user types their first question.
    SystemInfo::start_background_gather();

    // Create the GTK application.
    auto app = Gtk::Application::create("org.asktux.asktux");

    // Window must be created after the GApplication::startup signal fires.
    // Using signal_activate() is the standard gtkmm4 pattern.
    app->signal_activate().connect([app]() {
        static auto window = new MainWindow();
        window->set_application(app);
        window->show();
    });

    int result = app->run(argc, argv);

    // Cleanup libcurl.
    curl_global_cleanup();

    return result;
}
