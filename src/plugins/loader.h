#ifndef PLUGINS_LOADER_H
#define PLUGINS_LOADER_H

#include "plugin.h"
#include "list.h"
#include "../chat/interface.h"
#include "../logging/logging.h"
#include <string>
#include <vector>
#include <mutex>

namespace plugins {

class PluginLoader : public PluginList {
private:
    logging::LogSource log;
    std::mutex lock;
    std::vector<Plugin*> loaded_plugins;

public:
    PluginLoader(chat::ChatInterface *chat_if);
    ~PluginLoader();
    void loadPlugins(std::string path);
    std::vector<plugin_basic_info_t> getPlugins();
    std::string getPluginName(unsigned int index);
    std::string getPluginVersion(unsigned int index);
    std::string getPluginAuthor(unsigned int index);
    std::string getPluginDescription(unsigned int index);
    std::string getPluginAccentColor(unsigned int index);
    std::string getPluginWebsite(unsigned int index);
    std::string getPluginCopyright(unsigned int index);
    std::string getPluginLicense(unsigned int index);
    std::filesystem::path getPluginPath(unsigned int index);
    QWidget* getPluginSettingsPage(unsigned int index);
    int getPluginAPIVersion(unsigned int index);
    plugin_basic_info_t getPluginInfo(unsigned int index);
};

}

#endif // PLUGINS_LOADER_H
