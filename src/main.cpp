#include "gui/main_window.h"
#include "plugins/loader.h"
#include "chat/system.h"
#include "logging/logging.h"

#include <QApplication>
#include <cstdlib>
#include <string>
#include <iostream>

int main(int argc, char *argv[]) {
    char* home_path = getenv("HOME");
    // Configure logging system
    logging::add_output_stream(&std::clog, logging::INFO, logging::LINUX, true, logging::ANSI_ESCAPE_CODES);
    if (home_path != NULL)
        logging::add_output_file(std::string(home_path) + "/.local/share/streaming-toolbox/streaming-toolbox.log", logging::INFO, logging::LINUX, false, logging::NONE);
    logging::source log("Main");
    // Init other things
    QApplication a(argc, argv);
    chat::system chat_system;
    plugins::loader plugin_loader(&chat_system);

    // Load user plugins
    if (home_path != NULL)
        plugin_loader.load_plugins(std::string(home_path) + "/.local/share/streaming-toolbox/plugins");

    // Start GUI
    gui::main_window w(&plugin_loader, &chat_system);
    w.show();
    return a.exec();
}
