#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "pluginlist.h"
#include <QObject>
#include <QListWidget>
#include <QString>
#include <QListWidgetItem>
#include <QIcon>
#include <QPixmap>
#include <QColor>

#define PLUGIN_ACCENT_COLOR_SIZE 16

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);
    QObject::connect(ui->pluginList, &QListWidget::currentRowChanged, this, &MainWindow::selectPlugin);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::getPlugins(PluginList *plugin_list) {
    // Get list of plugins
    this->plugin_list = plugin_list;
    // Place all plugins into the UI's list
    for (auto plugin:plugin_list->getPlugins()) {
        QPixmap color_pix(PLUGIN_ACCENT_COLOR_SIZE, PLUGIN_ACCENT_COLOR_SIZE);
        color_pix.fill(QColor(plugin.accent_color.c_str()));
        ui->pluginList->addItem(new QListWidgetItem(QIcon(color_pix), plugin.name.c_str(), ui->pluginList));
    }
}

void MainWindow::selectPlugin(int index) {
    // Get info about that plugin and its settings page
    plugin_basic_info_t info = this->plugin_list->getPluginInfo(index);
    QWidget *settings = this->plugin_list->getPluginSettingsPage(index);
    // Update UI to match this info
    ui->pluginSettingsLabel->setText(("Settings for " + info.name).c_str());
    // Show plugin's settings page, or a blank page if it doesn't have one
    if (settings == nullptr)
        ui->pluginSettingsTab = &this->empty_plugin_settings_page;
    else
        ui->pluginSettingsTab = settings;
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
    }
    ui->pluginAboutText->setHtml(about_text);
}
