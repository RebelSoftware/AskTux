#include "MainWindow.h"
#include "Config.h"

#include <gtkmm.h>
#include <iostream>
#include <curl/curl.h>

int main(int argc, char* argv[])
{
    // Initialise libcurl globally (once per process).
    curl_global_init(CURL_GLOBAL_ALL);

    // Load saved configuration.
    Config::instance().load();

    // Create the GTK application.
    auto app = Gtk::Application::create("org.linhelp.linhelp");

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
