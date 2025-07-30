#include "event_viewer.h"
#include "ui_event_viewer.h"
#include "../libstrtb/event/system.h"
#include "../libstrtb/json/value_utils.h"
#include <QGuiApplication>
#include <QClipboard>

using namespace strtb::gui;

event_viewer::event_viewer(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::event_viewer) {
    ui->setupUi(this);
    QObject::connect(ui->button_refresh, &QPushButton::clicked, this, &event_viewer::populate);
    QObject::connect(ui->item_tree, &QTreeWidget::itemSelectionChanged, this, &event_viewer::show_info);
    populate();
}

event_viewer::~event_viewer() {
    delete ui;
}

void event_viewer::populate() {
    ui->item_tree->clear();
    event::item_info info = event::system_ptr->info(STRTB_EVENT_ROOT);
    tree_item* item = new tree_item();
    ui->item_tree->addTopLevelItem(item);
    item->event_item_info.resource_id = STRTB_EVENT_ROOT;
    item->event_item_info.type = info.type;
    item->event_item_info.display_name = info.display_name;
    item->event_item_info.description = info.description;
    item->event_item_info.provider_id = info.provider_id;
    item->setText(0, QString::fromStdString(info.display_name));
    item->populate();
    ui->item_tree->expandItem(item);
}

void event_viewer::tree_item::populate() {
    auto list = event::system_ptr->list(event_item_info.resource_id);
    for (auto& i : list) {
        tree_item* item = new tree_item();
        this->addChild(item);
        item->event_item_info = i;
        item->setText(0, QString::fromStdString(i.display_name));
        if (i.type == event::ITEM_CATEGORY)
            item->populate();
    }
}

static void list_param(QString& text, const strtb::event::param_definition& p) {
        text.append("<li><b>");
        text.append(QString::fromStdString(p.name));
        text.append(":</b> ");

        text.append(QString::fromStdString(strtb::json::type_to_string(p.type)));

        if (p.required)
            text.append(", required");
        else
            text.append(", optional");
        text.append("<br>Description: ");
        text.append(QString::fromStdString(p.description));

        if (!p.object_definition.empty()) {
            text.append("<br><b>Object definition:</b><ul>");
            for (auto po : p.object_definition)
                list_param(text, *po);
            text.append("</ul>");
        }

        if (p.array_definition) {
            text.append("<br><b>Array definition:</b><ul>");
            list_param(text, *p.array_definition);
            text.append("</ul>");
        }

        text.append("</li>");
}

static void list_params(QString& text, const std::vector<strtb::event::param_definition>& params) {
    text.append("<ul>");
    for (auto& p : params)
        list_param(text, p);
    text.append("</ul>");
}

static const char* item_type_to_str(strtb::event::item_type type) {
    switch (type) {
    case strtb::event::ITEM_UNDEFINED:
        return "Undefined";
        break;
    case strtb::event::ITEM_CATEGORY:
        return "Category";
        break;
    case strtb::event::ITEM_EVENT_SRC:
        return "Event Source";
        break;
    case strtb::event::ITEM_ACTION_SINK:
        return "Action Sink";
        break;
    default:
        return "Invalid Type";
    }
}

void event_viewer::tree_item::get_path(event::item_path &path) {
    if (event_item_info.resource_id == STRTB_EVENT_ROOT)
        return;
    ((tree_item*) parent())->get_path(path);
    path.push_back(event_item_info.name);
}

void event_viewer::show_info() {
    auto selected_items = ui->item_tree->selectedItems();

    if (selected_items.size() == 1) {
        // Show item info
        tree_item* item = (tree_item*) selected_items.value(0);
        const auto& info = item->event_item_info;

        event::item_path path;
        item->get_path(path);
        try {
            ui->item_path->setText(QString::fromStdString(event::item_path_to_string(path)));
            ui->item_path_copy->setEnabled(true);
        } catch (event::invalid_path& e) {
            ui->item_path->setText("invalid path: ");
            ui->item_path->insert(e.what());
            ui->item_path_copy->setDisabled(true);
        }

        QString text;
        text.append("<h2>Basic Info</h2>");
        text.append("<p><b>Resource ID:</b> ");
        text.append(QString::number(info.resource_id));
        text.append("</p>");
        text.append("<p><b>Provider ID:</b> ");
        text.append(QString::number(info.provider_id));
        text.append("</p>");
        text.append("<p><b>Name:</b> ");
        text.append(QString::fromStdString(info.name));
        text.append("</p>");
        text.append("<p><b>Display Name:</b> ");
        text.append(QString::fromStdString(info.display_name));
        text.append("</p>");
        text.append("<p><b>Description:</b> ");
        text.append(QString::fromStdString(info.description));
        text.append("</p>");
        text.append("<p><b>Type:</b> ");
        text.append(item_type_to_str(info.type));
        text.append("</p>");

        if (info.type == event::ITEM_EVENT_SRC || info.type == event::ITEM_ACTION_SINK) {
            text.append("<h2>Parameters</h2>");
            list_params(text, info.params);

            text.append("<h2>Returns</h2>");
            list_params(text, info.returns);

            text.append("<h2>Examples</h2> <ul>");
            for (auto& i : info.examples) {
                text.append("<li><b>Parameters:</b><pre><code>");
                text.append(QString::fromStdString(i.params.write_to_string(2)).toHtmlEscaped());
                text.append("</code></pre><br><b>Returned data:</b><pre><code>");
                text.append(QString::fromStdString(i.returns.write_to_string(2)).toHtmlEscaped());
                text.append("</code></pre></li>");
            }
            text.append("</ul>");
        }

        if (info.type == event::ITEM_CATEGORY) {
            text.append("<h2>Waiting Path Followers</h2><ul>");
            for (const auto& path_fl : event::system_ptr->info_path_followers(info.resource_id)) {
                text.append("<li><b>");
                text.append(QString::fromStdString(path_fl.owner_name));
                text.append(" (");
                text.append(QString::number(path_fl.sub_rid));
                text.append("):</b><br>Wants: ");
                text.append(item_type_to_str(path_fl.wanted_type));
                text.append("<br>Path: ");
                text.append(QString::fromStdString(event::item_path_to_string(path_fl.path)));
                text.append("<br>Status: ");

                switch (path_fl.status) {
                case event::PATH_FL_UNDEFINED:
                    text.append("Undefined");
                    break;
                case event::PATH_FL_READY:
                    text.append("Ready");
                    break;
                case event::PATH_FL_WAITING:
                    text.append("Waiting");
                    break;
                case event::PATH_FL_WRONG_TYPE:
                    text.append("Wrong Type");
                    break;
                case event::PATH_FL_BAD_PARAM:
                    text.append("Bad Parameter");
                    break;
                default:
                    text.append("Invalid Status");
                }
                if (!path_fl.diagnostic_info.empty()) {
                    text.append("<br>Diagnostic Info: ");
                    text.append(QString::fromStdString(path_fl.diagnostic_info));
                }

                text.append("</li>");
            }
            text.append("</ul>");
        }

        if (info.type == event::ITEM_EVENT_SRC) {
            text.append("<h2>Event Subscriptions</h2><ul>");
            for (const auto& event_sub : event::system_ptr->info_event_subs(info.resource_id)) {
                text.append("<li><b>");
                text.append(QString::fromStdString(event_sub.listener_name));
                text.append("(");
                text.append(QString::number(event_sub.event_sub_rid));
                text.append("): ");

                if (event_sub.param.type() == json::VAL_UNDEFINED) {
                    text.append("param not set</li>");
                } else {
                    text.append("param = <pre><code>");
                    text.append(QString::fromStdString(event_sub.param.value()->write_to_string()).toHtmlEscaped());
                    text.append("</code></pre></li>");
                }
            }
            text.append("</ul>");
        }

        ui->item_info->setHtml(text);
    } else {
        // Don't show info if either nothing or multiple items are selected
        ui->item_info->clear();
        ui->item_path->clear();
        ui->item_path_copy->setDisabled(true);
    }
}


void event_viewer::on_item_path_copy_clicked() {
    QGuiApplication::clipboard()->setText(ui->item_path->text());
}
