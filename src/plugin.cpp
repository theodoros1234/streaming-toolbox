#include "plugin.h"
#include "pluginlink.h"
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <dlfcn.h>

namespace fs = std::filesystem;

Plugin::Plugin(fs::path path) {
    this->path = path;

    // Load the plugin
    std::cout << "Loading plugin at " << path << std::endl;
    this->library = dlopen(path.c_str(), RTLD_NOW);
    if (this->library == NULL)
        // Plugin wasn't loaded
        throw std::runtime_error(dlerror());

    // Get needed functions from plugin
    void *ptr_exchange_info = dlsym(this->library, "plugin_exchange_info");
    void *ptr_activate = dlsym(this->library, "plugin_activate");
    void *ptr_deactivate = dlsym(this->library, "plugin_deactivate");

    if (!ptr_exchange_info || !ptr_activate || !ptr_deactivate)
        // Some required functions are missing from plugin
        throw std::runtime_error("Missing functions");

    this->functions.exchange_info = reinterpret_cast<plugin_info_t* (*)(plugin_callback_t*)>(ptr_exchange_info);
    this->functions.activate = reinterpret_cast<int (*)()>(ptr_activate);
    this->functions.deactivate = reinterpret_cast<void (*)()>(ptr_deactivate);

    // Exchange basic info with plugin
    this->info = this->functions.exchange_info(nullptr);
}

Plugin::~Plugin() {
    dlclose(this->library);
}

std::string Plugin::getName() {
    return this->info->name;
}

std::string Plugin::getVersion() {
    return this->info->version;
}

std::string Plugin::getAuthor() {
    return this->info->author;
}

std::string Plugin::getDescription() {
    return this->info->description;
}

std::string Plugin::getAccentColor() {
    return this->info->accent_color;
}

std::string Plugin::getWebsite() {
    return this->info->website;
}

std::string Plugin::getCopyright() {
    return this->info->copyright;
}

std::string Plugin::getLicense() {
    return this->info->license;
}

std::filesystem::path Plugin::getPath() {
    return this->path;
}

QWidget* Plugin::getSettingsPage() {
    return this->info->settings_page;
}

plugin_basic_info_t Plugin::getInfo() {
    return plugin_basic_info_t {
        .name = info->name,
        .version = info->version,
        .author = info->author,
        .description = info->description,
        .accent_color = info->accent_color,
        .website = info->website,
        .copyright = info->copyright,
        .license = info->license,
        .path = path
    };
}
