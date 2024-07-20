#ifndef PLUGIN_H
#define PLUGIN_H

#include "qwidget.h"
#include "pluginlink.h"
#include <filesystem>

struct plugin_basic_info_t {
    std::string name, version, author, description, accent_color, website, copyright, license;
    std::filesystem::path path;
};

class Plugin {
private:
    void *library = nullptr;
    struct plugin_info_t *info;
    std::filesystem::path path;
    struct {
        struct plugin_info_t* (*exchange_info)(plugin_callback_t*);
        int (*activate)();
        void (*deactivate)();
    } functions;

public:
    Plugin(std::filesystem::path path);
    ~Plugin();
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
    plugin_basic_info_t getInfo();
};

#endif // PLUGIN_H
