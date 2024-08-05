#ifndef PLUGINS_PLUGIN_H
#define PLUGINS_PLUGIN_H

#include "link.h"
#include "../chat/interface.h"
#include "../logging/logging.h"
#include <filesystem>
#include <set>
#include <mutex>
#include <QWidget>

struct plugin_basic_info_t {
    std::string name, version, author, description, accent_color, website, copyright, license;
    std::filesystem::path path;
    int api_version;
};

class Plugin {
private:
    static plugin_interface_t plugin_interface;
    void *library = nullptr;
    plugin_info_t info;
    int api_version;
    std::filesystem::path path;
    struct {
        int (*get_api_version)();
        bool (*send_api_version)(int version);
        plugin_info_t (*exchange_info)(plugin_interface_t interface, plugin_instance_t instance);
        int (*activate)();
        void (*deactivate)();
    } functions;

public:
    // The following have to be public, in order to be accessible by the plugin interface, but they MUST NOT be used by anything else.
    logging::LogSource log_if, log_pl;
    std::set<ChatProvider*> chat_providers;
    std::set<ChatChannel*> chat_channels;
    std::set<ChatSubscription*> chat_subscriptions;
    std::mutex chat_providers_lock, chat_channels_lock, chat_subscriptions_lock;
    static ChatInterface *chat_if;

    // Regular public members
    static void setInterfaces(ChatInterface *chat_if);
    Plugin(std::filesystem::path path);
    ~Plugin();
    void activate();
    std::string getName();
    std::string getVersion();
    std::string getAuthor();
    std::string getDescription();
    std::string getAccentColor();
    std::string getWebsite();
    std::string getCopyright();
    std::string getLicense();
    std::filesystem::path getPath();
    QWidget* getSettingsPage();
    int getAPIVersion();
    plugin_basic_info_t getInfo();
};

#endif // PLUGINS_PLUGIN_H
