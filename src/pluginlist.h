#ifndef PLUGINLIST_H
#define PLUGINLIST_H

#include <QWidget>
#include <vector>
#include <string>
#include <filesystem>
#include "plugin.h"

class PluginList {
public:
    virtual std::vector<plugin_basic_info_t> getPlugins() = 0;
    virtual std::string getPluginName(unsigned int index) = 0;
    virtual std::string getPluginVersion(unsigned int index) = 0;
    virtual std::string getPluginAuthor(unsigned int index) = 0;
    virtual std::string getPluginDescription(unsigned int index) = 0;
    virtual std::string getPluginAccentColor(unsigned int index) = 0;
    virtual std::string getPluginWebsite(unsigned int index) = 0;
    virtual std::string getPluginCopyright(unsigned int index) = 0;
    virtual std::string getPluginLicense(unsigned int index) = 0;
    virtual std::filesystem::path getPluginPath(unsigned int index) = 0;
    virtual QWidget* getPluginSettingsPage(unsigned int index) = 0;
    virtual int getPluginAPIVersion(unsigned int index) = 0;
    virtual plugin_basic_info_t getPluginInfo(unsigned int index) = 0;
};

#endif // PLUGINLIST_H
