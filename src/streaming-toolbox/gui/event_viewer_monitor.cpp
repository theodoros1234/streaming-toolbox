#include "event_viewer_monitor.h"
#include "ui_event_viewer_monitor.h"
#include "../libstrtb/json/value.h"
#include <QPushButton>

using namespace strtb::gui;

event_viewer_monitor::event_viewer_monitor(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::event_viewer_monitor) {
    ui->setupUi(this);
    QObject::connect(ui->button_sub, &QPushButton::clicked, this, &event_viewer_monitor::subscribe);
}

event_viewer_monitor::~event_viewer_monitor() {
    delete ui;
}

void event_viewer_monitor::show_with_item(const event::item_info& info, const event::item_path& path) {
    // TODO: unsub from previous event and clear stuff
    ui->event_log->clear();
    ui->param_bool->setChecked(false);
    ui->param_int->setValue(0);
    ui->param_string->clear();
    ui->param_omit->setChecked(false);
    _param.type = json::VAL_UNDEFINED;

    // New item
    _path = path;
    ui->label_name->setText(QString::fromStdString(info.display_name));
    ui->label_path->setText(QString::fromStdString(event::item_path_to_string(path)));
    ui->param_group->setHidden(info.params.empty());
    if (info.params.empty()) {
        ui->label_param->setText("none");
    } else {
        _param = info.params.back();
        QString p_str(QString::fromStdString(json::type_to_string(_param.type)));
        if (_param.required)
            p_str.append(", required");
        else
            p_str.append(", optional");
        ui->label_param->setText(p_str);
        ui->param_omit->setDisabled(_param.required);
        ui->param_bool->setVisible(_param.type == json::VAL_BOOL);
        ui->param_int->setVisible(_param.type == json::VAL_INT);
        ui->param_string->setVisible(_param.type == json::VAL_STRING);
    }
    show();
    activateWindow();
    raise();
}

void event_viewer_monitor::subscribe() {

}
