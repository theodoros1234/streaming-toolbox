#ifndef PLUGINS_LOADER_H
#define PLUGINS_LOADER_H

#include "plugin.h"
#include "list.h"
#include "../logging/logging.h"
#include <string>
#include <vector>
#include <mutex>

namespace strtb::plugins {

class loader : public list {
private:
    logging::source log;
    std::mutex lock;
    std::vector<plugin*> loaded_plugins;

public:
    loader();
    ~loader();
    void load_plugins(std::string path);
    std::vector<plugin_basic_info> plugins();
    std::string plugin_name(unsigned int index);
    std::string plugin_version(unsigned int index);
    std::string plugin_author(unsigned int index);
    std::string plugin_description(unsigned int index);
    std::string plugin_accent_color(unsigned int index);
    std::string plugin_website(unsigned int index);
    std::string plugin_copyright(unsigned int index);
    std::string plugin_license(unsigned int index);
    std::filesystem::path plugin_path(unsigned int index);
    QWidget* plugin_settings_page(unsigned int index);
    plugin_basic_info plugin_info(unsigned int index);
};

}

#endif // PLUGINS_LOADER_H
