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
    virtual std::vector<plugin_basic_info> plugins() = 0;
    virtual std::string plugin_name(unsigned int index) = 0;
    virtual std::string plugin_version(unsigned int index) = 0;
    virtual std::string plugin_author(unsigned int index) = 0;
    virtual std::string plugin_description(unsigned int index) = 0;
    virtual std::string plugin_accent_color(unsigned int index) = 0;
    virtual std::string plugin_website(unsigned int index) = 0;
    virtual std::string plugin_copyright(unsigned int index) = 0;
    virtual std::string plugin_license(unsigned int index) = 0;
    virtual std::filesystem::path plugin_path(unsigned int index) = 0;
    virtual QWidget* plugin_settings_page(unsigned int index) = 0;
    virtual plugin_basic_info plugin_info(unsigned int index) = 0;
};

}

#endif // PLUGINS_LIST_H
