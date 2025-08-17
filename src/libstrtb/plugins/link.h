#ifndef STRTB_PLUGINS_LINK_H
#define STRTB_PLUGINS_LINK_H

#include <string>
#include <QWidget>

namespace strtb::plugins {

// Information about the plugin that will be sent to Stream Toolbox
struct plugin_info {
    std::string name, version, author, description, accent_color, website, copyright, license;
    QWidget *settings_page;
};

}

#endif // STRTB_PLUGINS_LINK_H
