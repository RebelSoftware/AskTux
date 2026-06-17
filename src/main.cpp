#include "MainWindow.h"
#include "Config.h"
#include "SystemInfo.h"
#include "Log.h"
#include "ScopedTimer.h"

#include <gtkmm.h>
#include <iostream>
#include <curl/curl.h>
#include <cstring>
#include <glib.h>

/** Suppress harmless WebKit shutdown warnings about the web process name. */
static void silence_webkit_warnings(const gchar* log_domain,
                                    GLogLevelFlags log_level,
                                    const gchar* message,
                                    gpointer)
{
    // Discard "Error releasing name ... The connection is closed"
    if (log_level & G_LOG_LEVEL_WARNING && message
        && g_strstr_len(message, -1, "Error releasing name"))
        return;

    // Fall through to the default handler for everything else.
    g_log_default_handler(log_domain, log_level, message, nullptr);
}

int main(int argc, char* argv[])
{
    // Silence noisy WebKit shutdown warnings.
    // Must be installed before any WebKit objects are created.
    g_log_set_handler("GLib-GIO",
                      GLogLevelFlags(G_LOG_LEVEL_WARNING
                                     | G_LOG_FLAG_FATAL
                                     | G_LOG_FLAG_RECURSION),
                      silence_webkit_warnings, nullptr);

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
