#ifndef PLUGINS_LIST_H
#define PLUGINS_LIST_H

#include <QWidget>
#include <vector>
#include <string>
#include <filesystem>
#include "plugin.h"

namespace strtb::plugins {

class list {
protected:
    virtual ~list() = default;
public:
    virtual std::vector<plugin_basic_info> get_plugins() = 0;
    virtual std::string get_plugin_name(unsigned int index) = 0;
    virtual std::string get_plugin_version(unsigned int index) = 0;
    virtual std::string get_plugin_author(unsigned int index) = 0;
    virtual std::string get_plugin_description(unsigned int index) = 0;
    virtual std::string get_plugin_accent_color(unsigned int index) = 0;
    virtual std::string get_plugin_website(unsigned int index) = 0;
    virtual std::string get_plugin_copyright(unsigned int index) = 0;
    virtual std::string get_plugin_license(unsigned int index) = 0;
    virtual std::filesystem::path get_plugin_path(unsigned int index) = 0;
    virtual QWidget* get_plugin_settings_page(unsigned int index) = 0;
    virtual int get_plugin_api_version(unsigned int index) = 0;
    virtual plugin_basic_info get_plugin_info(unsigned int index) = 0;
};

}

#endif // PLUGINS_LIST_H
