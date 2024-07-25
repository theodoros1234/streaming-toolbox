#ifndef PLUGIN_H
#define PLUGIN_H

#include "qwidget.h"
#include "pluginlink.h"
#include "chatinterface.h"
#include <filesystem>

struct plugin_basic_info_t {
    std::string name, version, author, description, accent_color, website, copyright, license;
    std::filesystem::path path;
};

class Plugin {
private:
    static plugin_interface_t plugin_interface;
    void *library = nullptr;
    struct plugin_info_t *info;
    std::filesystem::path path;
    struct {
        struct plugin_info_t* (*exchange_info)(plugin_interface_t*);
        int (*activate)();
        void (*deactivate)();
    } functions;

public:
    static ChatInterface *chat_if;
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
    plugin_basic_info_t getInfo();
};

#endif // PLUGIN_H
