#include "plugin.h"
#include "../libstrtb/plugins/link.h"
#include "../libstrtb/logging/logging.h"
#include "../libstrtb/common/strescape.h"

#include <filesystem>
#include <stdexcept>
#include <dlfcn.h>

namespace fs = std::filesystem;
using namespace strtb;

namespace strtb::plugins {

static logging::source log("Plugin Loader", false);

plugin::plugin(fs::path path) {
    this->_path = path;

    // Load the plugin
    log.put(logging::INFO, {"Loading ", path.filename().c_str()});
    this->library = dlopen(path.c_str(), RTLD_NOW);
    if (this->library == NULL) {
        // Plugin wasn't loaded
        const char* error = dlerror();
        log.put(logging::ERROR, {"Failed loading ", path.filename(), ": ", error});
        throw std::runtime_error(error);
    }

    // Check plugins libstrtb version
    void *ptr_get_libstrtb_version = dlsym(this->library, "plugin_get_libstrtb_version");
    if (!ptr_get_libstrtb_version) {
        // libstrtb version check function are missing from the plugin
        log.put(logging::ERROR, {"Failed loading ", path.filename(), ": Couldn't check libstrtb version due to missing functions"});
        dlclose(this->library);
        throw std::runtime_error("Couldn't check libstrtb version due to missing functions");
    }
    this->functions.get_libstrtb_version = reinterpret_cast<common::version (*)()>(ptr_get_libstrtb_version);
    if (!common::versions_equal(this->functions.get_libstrtb_version(), common::get_libstrtb_version())) {
        // Streaming Toolbox doesn't support plugin's API version
        log.put(logging::ERROR, {"Failed loading ", path.filename(), ": Mismatched libstrtb versions between plugin and Streaming Toolbox"});
        dlclose(this->library);
        throw std::runtime_error("Mismatched libstrtb versions between plugin and Streaming Toolbox");
    }

    // Get needed functions from plugin
    void *ptr_exchange_info = dlsym(this->library, "plugin_exchange_info");
    void *ptr_activate = dlsym(this->library, "plugin_activate");
    void *ptr_deactivate = dlsym(this->library, "plugin_deactivate");

    if (!ptr_exchange_info || !ptr_activate || !ptr_deactivate) {
        // Some required functions are missing from plugin
        log.put(logging::ERROR, {"Failed loading ", path.filename(), ": Missing functions"});
        dlclose(this->library);
        throw std::runtime_error("Missing functions");
    }

    this->functions.exchange_info = reinterpret_cast<strtb::plugins::plugin_info (*)()>(ptr_exchange_info);
    this->functions.activate = reinterpret_cast<bool (*)()>(ptr_activate);
    this->functions.deactivate = reinterpret_cast<void (*)()>(ptr_deactivate);

    // Exchange basic info with plugin
    try {
        this->_info = this->functions.exchange_info();
    } catch (std::exception &e) {
        log.put(logging::ERROR, {"Unhandled exception while exchanging info with ", path.filename(), ": ", e.what()});
        throw;
    } catch (...) {
        log.put(logging::ERROR, {"Unhandled exception while exchanging info with ", path.filename()});
        throw;
    }

    log.put(logging::DEBUG, {"Loaded ", common::string_escape(this->_info.name)});
    try {
        this->activate();
    } catch (...) {
        dlclose(this->library);
        throw;
    }
}

plugin::~plugin() {
    // Unload plugin
    log.put(logging::DEBUG, {"Deactivating and unloading ", common::string_escape(this->_info.name)});
    try {
        this->functions.deactivate();
    } catch (std::exception &e) {
        log.put(logging::ERROR, {"Unhandled exception while deactivating ", common::string_escape(this->_info.name), ": ", e.what()});
    } catch (...) {
        log.put(logging::ERROR, {"Unhandled exception while deactivating ", common::string_escape(this->_info.name)});
    }
    dlclose(this->library);
}

void plugin::activate() {
    bool result;
    try {
        result = this->functions.activate();
    } catch (std::exception &e) {
        log.put(logging::ERROR, {"Unhandled exception while activating ", common::string_escape(this->_info.name), ": ", e.what()});
        throw;
    } catch (...) {
        log.put(logging::ERROR, {"Unhandled exception while activating ", common::string_escape(this->_info.name)});
        throw;
    }
    if (!result) {
        log.put(logging::ERROR, {"Couldn't activate ", common::string_escape(this->_info.name)});
        throw std::runtime_error("Couldn't activate " + common::string_escape(this->_info.name));
    }
}

std::string plugin::name() {
    return this->_info.name;
}

std::string plugin::version() {
    return this->_info.version;
}

std::string plugin::author() {
    return this->_info.author;
}

std::string plugin::description() {
    return this->_info.description;
}

std::string plugin::accent_color() {
    return this->_info.accent_color;
}

std::string plugin::website() {
    return this->_info.website;
}

std::string plugin::copyright() {
    return this->_info.copyright;
}

std::string plugin::license() {
    return this->_info.license;
}

std::filesystem::path plugin::path() {
    return this->_path;
}

QWidget* plugin::settings_page() {
    return this->_info.settings_page;
}

plugin_basic_info plugin::info() {
    return plugin_basic_info {
        .name = _info.name,
        .version = _info.version,
        .author = _info.author,
        .description = _info.description,
        .accent_color = _info.accent_color,
        .website = _info.website,
        .copyright = _info.copyright,
        .license = _info.license,
        .path = _path,
    };
}

}
