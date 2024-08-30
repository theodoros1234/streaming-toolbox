#ifndef STRTB_PLUGINS_LINK_H
#define STRTB_PLUGINS_LINK_H

#include <string>
#include <map>
#include <QWidget>

namespace strtb::plugins {

// Information about the plugin that will be sent to Stream Toolbox
struct plugin_info {
    std::string name, version, author, description, accent_color, website, copyright, license;
    QWidget *settings_page;
};

enum interfaces {INTERFACE_CHAT, INTERFACE_CONFIG};
typedef std::map<interfaces, void*> plugin_interface_map;

}

#endif // STRTB_PLUGINS_LINK_H
