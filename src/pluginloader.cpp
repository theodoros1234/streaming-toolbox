#include "pluginloader.h"

#include <vector>
#include <string>
#include <filesystem>
#include <iostream>
#include <dlfcn.h>
#include <exception>
#include "plugin.h"

namespace fs = std::filesystem;

PluginLoader::PluginLoader() {
    loaded_plugins = std::vector<Plugin*>();
}

void PluginLoader::loadPlugins(std::string path) {
    std::lock_guard guard(this->lock);
    // Check that path exists
    const fs::path path_obj(path);
    if (fs::exists(path_obj)) {
        std::cout << "Loading plugins from " << path_obj << std::endl;
        // Search path for libraries
        for (auto dir_entry : fs::directory_iterator(path_obj)) {
            if (dir_entry.is_directory()) {
                // Descend into directory which likely contains the plugin, and locate it in there
                auto plugin_dir = dir_entry.path();
                std::string plugin_possible_name = plugin_dir.filename().string();
                auto plugin_exec_path = plugin_dir/fs::path(plugin_possible_name + ".so");

                // Check that the target exec path really exists and is a file
                if (fs::exists(plugin_exec_path) && fs::is_regular_file(plugin_exec_path)) {
                    try {
                        // Try to load plugin
                        Plugin *plugin = new Plugin(plugin_exec_path);
                        loaded_plugins.push_back(plugin);
                        // Print plugin info
                        std::cout << "Loaded '" << plugin->getName() << "' v" << plugin->getVersion() << std::endl;
                    } catch (std::exception& e) {
                        std::cout << "Failed to load " << plugin_possible_name << ": " << e.what() << std::endl;
                    }
                }
            }
        }
    }
}

void PluginLoader::activatePlugins() {
    std::lock_guard guard(this->lock);
    std::cout << "Activating plugins" << std::endl;
    for (auto p=this->loaded_plugins.begin(); p<this->loaded_plugins.end(); p++) {
        try {
            (*p)->activate();
        } catch (std::exception& e) {
            std::cout << "Couldn't activate '" << (*p)->getName() << "', ignoring this plugin." << std::endl;
            this->loaded_plugins.erase(p);
        }
    }
}

PluginLoader::~PluginLoader() {
    std::lock_guard guard(this->lock);
    std::cout << "Deactivating plugins" << std::endl;
    // Deactivate and unload all plugins
    while (!this->loaded_plugins.empty()) {
        Plugin *p = loaded_plugins.back();
        loaded_plugins.pop_back();
        delete p;
    }
}

std::vector<plugin_basic_info_t> PluginLoader::getPlugins() {
    std::lock_guard guard(lock);
    std::vector<plugin_basic_info_t> basic_info;
    for (auto p:this->loaded_plugins)
        basic_info.push_back(p->getInfo());
    return basic_info;
}

std::string PluginLoader::getPluginName(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->getName();
    else
        return "";
}

std::string PluginLoader::getPluginVersion(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->getVersion();
    else
        return "";
}

std::string PluginLoader::getPluginAuthor(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->getAuthor();
    else
        return "";
}

std::string PluginLoader::getPluginDescription(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->getDescription();
    else
        return "";
}

std::string PluginLoader::getPluginAccentColor(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->getAccentColor();
    else
        return "";
}

std::string PluginLoader::getPluginWebsite(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->getWebsite();
    else
        return "";
}

std::string PluginLoader::getPluginCopyright(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->getCopyright();
    else
        return "";
}

std::string PluginLoader::getPluginLicense(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->getLicense();
    else
        return "";
}

std::filesystem::path PluginLoader::getPluginPath(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->getPath();
    else
        return "";
}

QWidget* PluginLoader::getPluginSettingsPage(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->getSettingsPage();
    else
        return nullptr;
}

plugin_basic_info_t PluginLoader::getPluginInfo(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->getInfo();
    else
        return plugin_basic_info_t();
}
