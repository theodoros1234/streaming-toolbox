#ifndef PLUGINS_LOADER_H
#define PLUGINS_LOADER_H

#include "plugin.h"
#include "list.h"
#include "../chat/interface.h"
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
    loader(chat::interface *chat_if);
    ~loader();
    void load_plugins(std::string path);
    std::vector<plugin_basic_info> get_plugins();
    std::string get_plugin_name(unsigned int index);
    std::string get_plugin_version(unsigned int index);
    std::string get_plugin_author(unsigned int index);
    std::string get_plugin_description(unsigned int index);
    std::string get_plugin_accent_color(unsigned int index);
    std::string get_plugin_website(unsigned int index);
    std::string get_plugin_copyright(unsigned int index);
    std::string get_plugin_license(unsigned int index);
    std::filesystem::path get_plugin_path(unsigned int index);
    QWidget* get_plugin_settings_page(unsigned int index);
    int get_plugin_api_version(unsigned int index);
    plugin_basic_info get_plugin_info(unsigned int index);
};

}

#endif // PLUGINS_LOADER_H
