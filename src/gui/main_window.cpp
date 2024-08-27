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

using namespace strtb;
using namespace strtb::gui;

main_window::main_window(plugins::list *plugin_list, chat::interface *chat_if, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , plugin_tab(plugin_list)
    , chat_tab(chat_if) {
    ui->setupUi(this);
    ui->statusbar->showMessage(common::get_libstrtb_version_string());
    ui->tabChat->layout()->addWidget(&chat_tab);
    ui->tabPlugins->layout()->addWidget(&plugin_tab);
}

main_window::~main_window() {
    delete ui;
}
