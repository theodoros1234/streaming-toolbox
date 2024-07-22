#ifndef PLUGINLINK_H
#define PLUGINLINK_H

#include <string>
#include <QWidget>

// Chat message
struct chat_message_t {
    bool is_reply;
    std::string user, user_color, message, replying_to_user, replying_to_message, reply_id;
};

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

// Functions of Stream Toolbox that the plugin can call for various reasons
// Plugin ===calls==> Stream Toolbox
struct plugin_callback_t {
    int whatever;
};

#endif // PLUGINLINK_H
