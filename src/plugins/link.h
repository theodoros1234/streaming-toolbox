#ifndef PLUGINS_LINK_H
#define PLUGINS_LINK_H

#define STRTB_PLUGIN_API_VERSION 3

#include <string>
#include <vector>
#include <map>
#include <QWidget>

namespace strtb_plugin {

// Information about the plugin that will be sent to Stream Toolbox
struct plugin_info {
    // User-visible info; will be visible to user in about page
    std::string name, version, author, description, accent_color, website, copyright, license;

    // Internally used by Stream Toolbox
    // Settings page
    QWidget *settings_page;
    // Functions of plugin that Stream Toolbox can call for various reasons
    // Stream Toolbox ===calls==> plugin
    // Callbacks for various events that the plugin can subscribe to
    //void (*chat_messages_received)(std::vector<struct chat_message> messages);
};

// Plugin instance
struct plugin_instance {
    void *ptr = nullptr;
};

// Chat message
struct chat_message {
    std::string provider_id, provider_name, channel_id, channel_name, user_id, user_name, user_color, message;
    bool is_mod=false, is_broadcaster=false, is_paid_member=false;
    long long int timestamp=0;
    std::map<std::string, std::string> more_metadata;
};

// Chat provider
struct chat_provider {
    void *ptr = nullptr;
};

// Chat channel
struct chat_channel {
    void *ptr = nullptr;
};

// Chat subscription
struct chat_subscription {
    void *ptr = nullptr;
};

// Chat channel info
struct chat_channel_info {
    std::string id, name;
};

// Chat provider info
struct chat_provider_info {
    std::string id, name;
    int channel_count;
    std::vector<chat_channel_info> channels;
};

// Chat interface channel info
struct chat_if_channel_info {
    int provider_count;
    int channel_count;
    std::vector<chat_provider_info> providers;
};

// Log message level
enum log_level {LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_CRITICAL};

// Functions of Stream Toolbox that the plugin can call for various reasons
// Plugin ===calls==> Stream Toolbox
struct plugin_interface {
    // Chat interface
    chat_if_channel_info (*chat_if_get_channel_info)();
    chat_provider (*chat_if_register_provider)(plugin_instance plugin, std::string id, std::string name);
    chat_subscription (*chat_if_subscribe)(plugin_instance plugin, std::string provider_id, std::string channel_id);
    // Chat provider
    std::string (*chat_provider_get_id)(chat_provider provider);
    std::string (*chat_provider_get_name)(chat_provider provider);
    chat_provider_info (*chat_provider_get_info)(chat_provider provider);
    chat_channel (*chat_provider_register_channel)(plugin_instance plugin, chat_provider provider, std::string id, std::string name);
    void (*chat_provider_delete)(plugin_instance plugin, chat_provider *provider);
    // Chat channel
    std::string (*chat_channel_get_id)(chat_channel channel);
    std::string (*chat_channel_get_name)(chat_channel channel);
    std::string (*chat_channel_get_provider_id)(chat_channel channel);
    std::string (*chat_channel_get_provider_name)(chat_channel channel);
    chat_channel_info (*chat_channel_get_info)(chat_channel channel);
    void (*chat_channel_push_one)(chat_channel channel, chat_message *message);
    void (*chat_channel_push_many)(chat_channel channel, std::vector<chat_message> *messages);
    void (*chat_channel_delete)(plugin_instance plugin, chat_channel *channel);
    // Chat subscription
    std::string (*chat_subscription_get_provider_id)(chat_subscription sub);
    std::string (*chat_subscription_get_channel_id)(chat_subscription sub);
    std::vector<chat_message> (*chat_subscription_pull)(chat_subscription sub);
    void (*chat_subscription_unsubscribe)(chat_subscription sub);
    void (*chat_subscription_delete)(plugin_instance plugin, chat_subscription *sub);
    // Logging
    void (*log_put)(plugin_instance plugin, log_level level, std::vector<std::string> message);
};

}

#endif // PLUGINS_LINK_H
