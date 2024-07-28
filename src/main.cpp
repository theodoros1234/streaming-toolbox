#include "mainwindow.h"
#include "pluginloader.h"
#include "chatsystem.h"
#include "logging.h"

#include <QApplication>
#include <cstdlib>
#include <string>
#include <iostream>

int main(int argc, char *argv[]) {
    char* home_path = getenv("HOME");
    // Configure logging system
    logging::addOutputStream(&std::clog, logging::INFO, logging::LINUX, true, logging::ANSI_ESCAPE_CODES);
    if (home_path != NULL)
        logging::addOutputFile(std::string(home_path) + "/.local/share/streaming-toolbox/streaming-toolbox.log", logging::INFO, logging::LINUX, false, logging::NONE);
    logging::LogSource log("Main");
    // Init other things
    QApplication a(argc, argv);
    ChatSystem chat_system;
    PluginLoader plugin_loader((ChatInterface*)&chat_system);
    MainWindow w;

    // Load user plugins
    if (home_path != NULL)
        plugin_loader.loadPlugins(std::string(home_path) + "/.local/share/streaming-toolbox/plugins");
    plugin_loader.activatePlugins();

    // Start GUI
    w.getPlugins(&plugin_loader);
    w.show();
    return a.exec();
}
