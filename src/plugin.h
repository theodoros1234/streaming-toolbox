#ifndef PLUGIN_H
#define PLUGIN_H

#include "qwidget.h"
#include "pluginlink.h"
#include "chatinterface.h"
#include "logging.h"
#include <filesystem>

struct plugin_basic_info_t {
    std::string name, version, author, description, accent_color, website, copyright, license;
    std::filesystem::path path;
    int api_version;
};

class Plugin {
private:
    logging::LogSource log_if, log_pl;
    static plugin_interface_t plugin_interface;
    void *library = nullptr;
    struct plugin_info_t *info;
    int api_version;
    std::filesystem::path path;
    struct {
        int (*get_api_version)();
        bool (*send_api_version)(int version);
        struct plugin_info_t* (*exchange_info)(plugin_interface_t* interface);
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
    int getAPIVersion();
    plugin_basic_info_t getInfo();
};

#endif // PLUGIN_H
