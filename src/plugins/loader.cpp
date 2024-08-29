#include "loader.h"
#include "plugin.h"

#include <vector>
#include <string>
#include <filesystem>
#include <dlfcn.h>
#include <exception>

namespace fs = std::filesystem;
using namespace strtb;
using namespace strtb::plugins;

loader::loader() : log("Plugin Loader") {}

void loader::load_plugins(std::string path) {
    std::lock_guard guard(this->lock);
    // Check that path exists
    const fs::path path_obj(path);
    if (fs::exists(path_obj)) {
        log.put(logging::INFO, {"Loading plugins from ", path_obj});
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
                        class plugin *plugin = new class plugin(plugin_exec_path);
                        loaded_plugins.push_back(plugin);
                    } catch (std::exception& e) {/* Skip on error */}
                }
            }
        }
    }
}

loader::~loader() {
    std::lock_guard guard(this->lock);
    this->log.put(logging::DEBUG, {"Deactivating and unloading plugins"});
    // Deactivate and unload all plugins
    while (!this->loaded_plugins.empty()) {
        delete loaded_plugins.back();
        loaded_plugins.pop_back();
    }
}

std::vector<plugin_basic_info> loader::plugins() {
    std::lock_guard guard(lock);
    std::vector<plugin_basic_info> basic_info;
    for (auto p : this->loaded_plugins)
        basic_info.push_back(p->info());
    return basic_info;
}

std::string loader::plugin_name(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->name();
    else
        return "";
}

std::string loader::plugin_version(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->version();
    else
        return "";
}

std::string loader::plugin_author(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->author();
    else
        return "";
}

std::string loader::plugin_description(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->description();
    else
        return "";
}

std::string loader::plugin_accent_color(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->accent_color();
    else
        return "";
}

std::string loader::plugin_website(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->website();
    else
        return "";
}

std::string loader::plugin_copyright(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->copyright();
    else
        return "";
}

std::string loader::plugin_license(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->license();
    else
        return "";
}

std::filesystem::path loader::plugin_path(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->path();
    else
        return "";
}

QWidget* loader::plugin_settings_page(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->settings_page();
    else
        return nullptr;
}

plugin_basic_info loader::plugin_info(unsigned int index) {
    std::lock_guard guard(lock);
    if (index < this->loaded_plugins.size())
        return this->loaded_plugins[index]->info();
    else
        return plugin_basic_info();
}
