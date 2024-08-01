#ifndef PLUGINLINK_H
#define PLUGINLINK_H

#define STRTB_PLUGIN_API_VERSION 1

#include <string>
#include <QWidget>

// Information about the plugin that will be sent to Stream Toolbox
struct plugin_info_t {
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

// Chat message
struct chat_message_t {
    std::string provider_id, provider_name, channel_id, channel_name, user_id, user_name, user_color, message;
    bool is_mod=false, is_broadcaster=false, is_paid_member=false;
    long long int timestamp=0;
    std::map<std::string, std::string> more_metadata;
};

// Chat provider
struct chat_provider_t {
    void *ptr = nullptr;
};

// Chat channel
struct chat_channel_t {
    void *ptr = nullptr;
};

// Chat subscription
struct chat_subscription_t {
    void *ptr = nullptr;
};

// Chat channel info
struct chat_channel_info_t {
    std::string id, name;
};

// Chat provider info
struct chat_provider_info_t {
    std::string id, name;
    int channel_count;
    std::vector<chat_channel_info_t> channels;
};

// Chat interface channel info
struct chat_if_channel_info_t {
    int provider_count;
    int channel_count;
    std::vector<chat_provider_info_t> providers;
};

// Functions of Stream Toolbox that the plugin can call for various reasons
// Plugin ===calls==> Stream Toolbox
struct plugin_interface_t {
    // Chat interface
    chat_if_channel_info_t (*chat_if_get_channel_info)();
    chat_provider_t (*chat_if_register_provider)(std::string id, std::string name);
    chat_subscription_t (*chat_if_subscribe)(std::string provider_id, std::string channel_id);
    // Chat provider
    std::string (*chat_provider_get_id)(chat_provider_t *provider);
    std::string (*chat_provider_get_name)(chat_provider_t *provider);
    chat_provider_info_t (*chat_provider_get_info)(chat_provider_t *provider);
    chat_channel_t (*chat_provider_register_channel)(chat_provider_t *provider, std::string id, std::string name);
    void (*chat_provider_delete)(chat_provider_t *provider);
    // Chat channel
    std::string (*chat_channel_get_id)(chat_channel_t *channel);
    std::string (*chat_channel_get_name)(chat_channel_t *channel);
    std::string (*chat_channel_get_provider_id)(chat_channel_t *channel);
    std::string (*chat_channel_get_provider_name)(chat_channel_t *channel);
    chat_channel_info_t (*chat_channel_get_info)(chat_channel_t *channel);
    void (*chat_channel_push_one)(chat_channel_t *channel, chat_message_t *message);
    void (*chat_channel_push_many)(chat_channel_t *channel, std::vector<chat_message_t> *messages);
    void (*chat_channel_delete)(chat_channel_t *channel);
    // Chat subscription
    std::string (*chat_subscription_get_provider_id)(chat_subscription_t *sub);
    std::string (*chat_subscription_get_channel_id)(chat_subscription_t *sub);
    std::vector<chat_message_t> (*chat_subscription_pull)(chat_subscription_t *sub);
    void (*chat_subscription_unsubscribe)(chat_subscription_t *sub);
    void (*chat_subscription_delete)(chat_subscription_t *sub);
};

#endif // PLUGINLINK_H
