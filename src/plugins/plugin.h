#ifndef PLUGINS_PLUGIN_H
#define PLUGINS_PLUGIN_H

#include "link.h"
#include "../chat/interface.h"
#include "../logging/logging.h"
#include <filesystem>
#include <set>
#include <mutex>
#include <QWidget>

namespace strtb::plugins {

struct plugin_basic_info {
    std::string name, version, author, description, accent_color, website, copyright, license;
    std::filesystem::path path;
    int api_version;
};

class plugin {
private:
    static strtb_plugin::plugin_interface plugin_interface;
    void *library = nullptr;
    strtb_plugin::plugin_info info;
    int api_version;
    std::filesystem::path path;
    struct {
        int (*get_api_version)();
        bool (*send_api_version)(int version);
        strtb_plugin::plugin_info (*exchange_info)(strtb_plugin::plugin_interface interface, strtb_plugin::plugin_instance instance);
        int (*activate)();
        void (*deactivate)();
    } functions;

public:
    // The following have to be public, in order to be accessible by the plugin interface, but they MUST NOT be used by anything else.
    logging::source log_if, log_pl;
    std::set<chat::provider*> chat_providers;
    std::set<chat::channel*> chat_channels;
    std::set<chat::subscription*> chat_subscriptions;
    std::mutex chat_providers_lock, chat_channels_lock, chat_subscriptions_lock;
    static chat::interface *chat_if;

    // Regular public members
    static void set_interfaces(chat::interface *chat_if);
    plugin(std::filesystem::path path);
    ~plugin();
    void activate();
    std::string get_name();
    std::string get_version();
    std::string get_author();
    std::string get_description();
    std::string get_accent_color();
    std::string get_website();
    std::string get_copyright();
    std::string get_license();
    std::filesystem::path get_path();
    QWidget* get_settings_page();
    int get_api_version();
    plugin_basic_info get_info();
};

}

#endif // PLUGINS_PLUGIN_H
