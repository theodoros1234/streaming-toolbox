#include "plugin_tab.h"
#include "ui_plugin_tab.h"

#define PLUGIN_ACCENT_COLOR_SIZE 16

using namespace strtb;
using namespace strtb::gui;

plugin_tab::plugin_tab(plugins::list *plugin_list, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::plugin_tab) {
    // Setup UI
    ui->setupUi(this);
    placeholder_no_settings = new QLabel("This plugin has no settings page.");
    placeholder_no_settings->setWordWrap(true);
    placeholder_no_settings->setAlignment(Qt::AlignCenter);
    // Slots and signals
    QObject::connect(ui->pluginList, &QListWidget::currentRowChanged, this, &plugin_tab::select_plugin);
    // Get list of plugins
    this->plugin_list = plugin_list;
    // Place all plugins into the UI's list
    for (auto plugin : plugin_list->get_plugins()) {
        QPixmap color_pix(PLUGIN_ACCENT_COLOR_SIZE, PLUGIN_ACCENT_COLOR_SIZE);
        color_pix.fill(QColor(plugin.accent_color.c_str()));
        ui->pluginList->addItem(new QListWidgetItem(QIcon(color_pix), plugin.name.c_str(), ui->pluginList));
    }
}

plugin_tab::~plugin_tab() {
    // Remove plugin's settings page, so it doesn't get prematurely deleted
    if (this->ui->settingsScrollArea->widget())
        this->ui->settingsScrollArea->takeWidget();
    // Delete UI
    delete placeholder_no_settings;
    delete ui;
}

void plugin_tab::select_plugin(int index) {
    // Get info about that plugin and its settings page
    plugins::plugin_basic_info info = this->plugin_list->get_plugin_info(index);
    QWidget *settings = this->plugin_list->get_plugin_settings_page(index);

    // Update title
    ui->pluginSettingsLabel->setText(("Settings for " + info.name).c_str());

    // Remove previous settings page from other plugn (if it's there)
    if (this->ui->settingsScrollArea->widget())
        this->ui->settingsScrollArea->takeWidget();
    // Add this plugin's settings page, if it has one, otherwise add a placeholder
    if (settings)
        this->ui->settingsScrollArea->setWidget(settings);
    else
        this->ui->settingsScrollArea->setWidget(this->placeholder_no_settings);

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

