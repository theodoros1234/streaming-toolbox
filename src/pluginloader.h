#ifndef PLUGINLOADER_H
#define PLUGINLOADER_H

#include "plugin.h"
#include "pluginlist.h"
#include <string>
#include <vector>
#include <mutex>

class PluginLoader : public PluginList {
private:
    std::mutex lock;
    std::vector<Plugin*> loaded_plugins;

public:
    PluginLoader();
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
    plugin_basic_info_t getPluginInfo(unsigned int index);
};


#endif // PLUGINLOADER_H
