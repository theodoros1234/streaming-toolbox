#include "event_viewer.h"
#include "ui_event_viewer.h"
#include "../libstrtb/event/system.h"
#include "../libstrtb/json/value_utils.h"

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
    item->setText(0, info.display_name.c_str());
    item->populate();
    ui->item_tree->expandItem(item);
}

void event_viewer::tree_item::populate() {
    auto list = event::system_ptr->list(event_item_info.resource_id);
    for (auto& i : list) {
        tree_item* item = new tree_item();
        this->addChild(item);
        item->event_item_info = i;
        item->setText(0, i.display_name.c_str());
        if (i.type == event::ITEM_CATEGORY)
            item->populate();
    }
}

static void list_param(QString& text, const strtb::event::param_definition& p) {

        text.append("<li><b>");
        text.append(p.name);
        text.append(":</b> ");

        text.append(strtb::json::type_to_string(p.type));

        if (p.required)
            text.append(", required");
        else
            text.append(", optional");

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

void event_viewer::show_info() {
    auto selected_items = ui->item_tree->selectedItems();

    if (selected_items.size() == 1) {
        // Show item info
        const auto& info = ((tree_item*) selected_items.value(0))->event_item_info;
        QString text;
        text.append("<h2>Basic Info</h2>");
        text.append("<p><b>Resource ID:</b> ");
        text.append(QString::number(info.resource_id));
        text.append("</p>");
        text.append("<p><b>Provider ID:</b> ");
        text.append(QString::number(info.provider_id));
        text.append("</p>");
        text.append("<p><b>Name:</b> ");
        text.append(info.name);
        text.append("</p>");
        text.append("<p><b>Display Name:</b> ");
        text.append(info.display_name);
        text.append("</p>");
        text.append("<p><b>Description:</b> ");
        text.append(info.description);
        text.append("</p>");
        text.append("<p><b>Type:</b> ");
        switch (info.type) {
        case event::ITEM_UNDEFINED:
            text.append("Undefined");
            break;
        case event::ITEM_CATEGORY:
            text.append("Category");
            break;
        case event::ITEM_EVENT_SRC:
            text.append("Event Source");
            break;
        case event::ITEM_ACTION_SINK:
            text.append("Action Sink");
            break;
        default:
            text.append("Invalid Type (");
            text.append(QString::number(info.type));
            text.append(")");
        }
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

        ui->item_info->setHtml(text);
    } else {
        // Don't show info if either nothing or multiple items are selected
        ui->item_info->clear();
    }
}

