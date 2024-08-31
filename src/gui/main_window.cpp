#include "main_window.h"
#include "ui_main_window.h"
#include "../plugins/list.h"
#include "../common/version.h"

#include <QObject>
#include <QListWidget>
#include <QString>
#include <QListWidgetItem>
#include <QIcon>
#include <QPixmap>
#include <QColor>
#include <QResizeEvent>
#include <QWidget>
#include <Qt>

using namespace strtb;
using namespace strtb::gui;

static const std::string conf_cat = "gui_main_window";

main_window::main_window(plugins::list *plugin_list, chat::interface *chat_if, config::interface *config_if, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , plugin_tab(plugin_list)
    , chat_tab(chat_if)
    , config_if(config_if)
    , log("GUI: Main Window") {

    // Set up window and tabs
    ui->setupUi(this);
    ui->statusbar->showMessage(common::get_libstrtb_version_string());
    ui->tabChat->layout()->addWidget(&chat_tab);
    ui->tabPlugins->layout()->addWidget(&plugin_tab);

    // Load window config
    try {
        config_if->load_category(conf_cat);
        if (config_if->get_category_root_type(conf_cat) != json::VAL_OBJECT)
            config_if->set_category_root(conf_cat, json::VAL_OBJECT);
        if (config_if->get_type(conf_cat, {"window_width"}) == json::VAL_INT &&
            config_if->get_type(conf_cat, {"window_height"}) == json::VAL_INT) {
            json::value *width, *height;
            width = config_if->get_value(conf_cat, {"window_width"});
            height = config_if->get_value(conf_cat, {"window_height"});
            this->resize(((json::value_int*) width)->value(), ((json::value_int*) height)->value());
            delete width;
            delete height;
        }
        if (config_if->get_type(conf_cat, {"window_is_maximized"}) == json::VAL_BOOL) {
            json::value *maximized = config_if->get_value(conf_cat, {"window_is_maximized"});
            if (((json::value_bool*) maximized)->value())
                this->setWindowState(Qt::WindowMaximized);
            delete maximized;
        }
        is_config_loaded = true;
    } catch (std::exception &e) {
        log.put(logging::WARNING, {"Failed to load window config: ", e.what()});
    }
}

main_window::~main_window() {
    delete ui;
    if (is_config_loaded) {
        try {
            config_if->close_category(conf_cat);
        } catch (std::exception &e) {
            log.put(logging::WARNING, {"Failed to save window config: ", e.what()});
        }
    }
}

void main_window::resizeEvent(QResizeEvent *event) {
    if (!is_config_loaded)
        return;
    try {
        if (this->isMaximized()) {
            config_if->set_value(conf_cat, {"window_is_maximized"}, true);
        } else {
            config_if->set_value(conf_cat, {"window_is_maximized"}, false);
            config_if->set_value(conf_cat, {"window_width"}, event->size().width());
            config_if->set_value(conf_cat, {"window_height"}, event->size().height());
        }
    } catch (std::exception &e) {
        log.put(logging::DEBUG, {"Failed to save window size and state: ", e.what()});
    } catch (...) {}
}
