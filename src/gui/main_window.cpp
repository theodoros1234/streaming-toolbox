#include "main_window.h"
#include "ui_main_window.h"
#include "../plugins/list.h"

#include <QObject>
#include <QListWidget>
#include <QString>
#include <QListWidgetItem>
#include <QIcon>
#include <QPixmap>
#include <QColor>

#define PLUGIN_ACCENT_COLOR_SIZE 16

using namespace gui;

main_window::main_window(chat::interface *chat_if, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , chat_tab(chat_if, this) {
    ui->setupUi(this);
    QObject::connect(ui->pluginList, &QListWidget::currentRowChanged, this, &main_window::select_plugin);
    ui->tabChat->layout()->addWidget(&chat_tab);
}

main_window::~main_window() {
    delete ui;
}

void main_window::get_plugins(plugins::list *plugin_list) {
    // Get list of plugins
    this->plugin_list = plugin_list;
    // Place all plugins into the UI's list
    for (auto plugin : plugin_list->get_plugins()) {
        QPixmap color_pix(PLUGIN_ACCENT_COLOR_SIZE, PLUGIN_ACCENT_COLOR_SIZE);
        color_pix.fill(QColor(plugin.accent_color.c_str()));
        ui->pluginList->addItem(new QListWidgetItem(QIcon(color_pix), plugin.name.c_str(), ui->pluginList));
    }
}

void main_window::select_plugin(int index) {
    // Get info about that plugin and its settings page
    plugins::plugin_basic_info info = this->plugin_list->get_plugin_info(index);
    QWidget *settings = this->plugin_list->get_plugin_settings_page(index);
    // Update UI to match this info
    ui->pluginSettingsLabel->setText(("Settings for " + info.name).c_str());
    // Show plugin's settings page, or a blank page if it doesn't have one
    if (settings == nullptr) {
        ui->pluginSettingsTab = &this->empty_plugin_settings_page;
        this->empty_plugin_settings_page.setParent(ui->pluginView);
    } else {
        ui->pluginSettingsTab = settings;
        settings->setParent(ui->pluginView);
    }
    // Update about page, skipping any empty fields
    QString about_text;
    if (!info.name.empty()) {
        about_text.append("<b>Name:</b> ");
        about_text.append(QString(info.name.c_str()).toHtmlEscaped());
        about_text.append("<br><br>");
    }
    if (!info.version.empty()) {
        about_text.append("<b>Version:</b> ");
        about_text.append(QString(info.version.c_str()).toHtmlEscaped());
        about_text.append("<br><br>");
    }
    if (!info.author.empty()) {
        about_text.append("<b>Author:</b> ");
        about_text.append(QString(info.author.c_str()).toHtmlEscaped());
        about_text.append("<br><br>");
    }
    if (!info.description.empty()) {
        about_text.append("<b>Description:</b> ");
        about_text.append(QString(info.description.c_str()).toHtmlEscaped());
        about_text.append("<br><br>");
    }
    if (!info.website.empty()) {
        // Make the website link clickable
        QString link(info.website.c_str());
        link = link.toHtmlEscaped();
        about_text.append("<b>Website:</b> <a href=\"");
        about_text.append(link);
        about_text.append("\">");
        about_text.append(link);
        about_text.append("</a><br><br>");
    }
    if (!info.copyright.empty()) {
        about_text.append("<b>Copyright:</b> ");
        about_text.append(QString(info.copyright.c_str()).toHtmlEscaped());
        about_text.append("<br><br>");
    }
    if (!info.license.empty()) {
        about_text.append("<b>License:</b> ");
        about_text.append(QString(info.license.c_str()).toHtmlEscaped());
        about_text.append("<br><br>");
    }
    if (!info.path.empty()) {
        about_text.append("<b>Path:</b> ");
        about_text.append(QString(info.path.c_str()).toHtmlEscaped());
        about_text.append("<br><br>");
    }
    about_text.append("<b>API Version:</b> ");
    about_text.append(QString(std::to_string(info.api_version).c_str()));
    ui->pluginAboutText->setHtml(about_text);
}
