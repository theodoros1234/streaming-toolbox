#include "mainwindow.h"
#include "pluginloader.h"

#include <QApplication>
#include <cstdlib>
#include <string>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    MainWindow w;
    PluginLoader plugin_loader;

    // Load user plugins
    char* home_path = getenv("HOME");
    if (home_path != NULL)
        plugin_loader.loadPlugins(std::string(home_path) + "/.local/share/streaming-toolbox/plugins");
    plugin_loader.activatePlugins();

    // Start GUI
    w.getPlugins(&plugin_loader);
    w.show();
    return a.exec();
}
