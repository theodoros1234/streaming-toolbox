#ifndef STRTB_PLUGINS_PLUGIN_H
#define STRTB_PLUGINS_PLUGIN_H

#include "link.h"
#include "../chat/interface.h"
#include "../common/version.h"
#include "../config/interface.h"
#include <filesystem>
#include <QWidget>

namespace strtb::plugins {

struct plugin_basic_info {
    std::string name, version, author, description, accent_color, website, copyright, license;
    std::filesystem::path path;
};

class plugin {
private:
    void *library = nullptr;
    plugin_info _info;
    std::filesystem::path _path;
    struct {
        common::version (*get_libstrtb_version)();
        plugins::plugin_info (*exchange_info)(const plugin_interface_map &interface_map);
        bool (*activate)();
        void (*deactivate)();
    } functions;

public:
    static void set_interfaces(chat::interface *chat_if, config::interface *config_if);
    plugin(std::filesystem::path path);
    ~plugin();
    void activate();
    std::string name();
    std::string version();
    std::string author();
    std::string description();
    std::string accent_color();
    std::string website();
    std::string copyright();
    std::string license();
    std::filesystem::path path();
    QWidget* settings_page();
    plugin_basic_info info();
};

}

#endif // PLUGINS_PLUGIN_H
