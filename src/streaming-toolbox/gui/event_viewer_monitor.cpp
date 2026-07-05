#include "event_viewer_monitor.h"
#include "ui_event_viewer_monitor.h"
#include "../libstrtb/json/value.h"
#include "../libstrtb/logging.h"
#include <QPushButton>
#include <QKeyEvent>

using namespace strtb::gui;

strtb::logging::source log_s("GUI: Event Monitor");

event_viewer_monitor::event_viewer_monitor(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::event_viewer_monitor) {
    ui->setupUi(this);
    QObject::connect(ui->button_sub, &QPushButton::clicked, this, &event_viewer_monitor::subscribe);
    QObject::connect(&_listener.emitter, &event::event_listener_qt_signal_emitter::event_received,
                     this, &event_viewer_monitor::event_received, Qt::QueuedConnection);
    _scrollbar = ui->event_log->verticalScrollBar();
}

event_viewer_monitor::~event_viewer_monitor() {
    if (_sub_id)
        subscribe();    // will unsub
    delete ui;
}

void event_viewer_monitor::show_with_item(const event::item_info& info, const event::item_path& path) {
    // Unsub from previous event and clear stuff
    if (_sub_id)
        subscribe();    // will unsub
    ui->event_log->clear();
    ui->param_bool->setChecked(false);
    ui->param_int->setValue(0);
    ui->param_string->clear();
    ui->param_omit->setChecked(false);
    _param.type = json::VAL_UNDEFINED;

    // New item
    _path = path;
    ui->label_name->setText(QString::fromStdString(info.display_name));
    ui->label_path->setText(QString::fromStdString((path.to_string())));
    ui->param_group->setHidden(info.params.empty());
    if (info.params.empty()) {
        ui->label_param->setText("none");
    } else {
        _param = info.params.back();
        QString p_str(json::type_to_string(_param.type));
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
    if (_sub_id) {  // Already subbed, unsub
        _listener.unsubscribe(_sub_id);
        ui->button_sub->setText("&Subscribe");
        _sub_id = 0;
    } else {        // Not subbed, sub now
        ui->event_log->clear();
        if (_param.type == json::VAL_UNDEFINED || ui->param_omit->isChecked()) {
            _sub_id = _listener.subscribe(_path);
        } else {
            switch (_param.type) {
            case json::VAL_BOOL:
                _sub_id = _listener.subscribe(_path, ui->param_bool->isChecked());
                break;

            case json::VAL_INT:
                _sub_id = _listener.subscribe(_path, ui->param_int->value());
                break;

            case json::VAL_STRING:
                _sub_id = _listener.subscribe(_path, ui->param_string->text().toStdString());
                break;

            default:
                log_s.error({"Cannot subscribe, holding an invalid parameter type: ", json::type_to_string(_param.type)});
            }
        }

        ui->button_sub->setText("Un&subscribe");
    }

    ui->param_group->setDisabled(_sub_id);
}

void event_viewer_monitor::closeEvent(QCloseEvent*) {
    if (_sub_id)
        subscribe();    // will unsub
}

void event_viewer_monitor::event_received(uint64_t, json::holder event) {
    ui->event_log->append(QString::fromStdString(event.value()->write_to_string()));
}

void event_viewer_monitor::on_param_omit_checkStateChanged(Qt::CheckState state) {
    bool checked = state == Qt::Checked;
    ui->param_bool->setDisabled(checked);
    ui->param_int->setDisabled(checked);
    ui->param_string->setDisabled(checked);
}


void event_viewer_monitor::on_button_scroll_clicked() {
    _scrollbar->setValue(_scrollbar->maximum());
}

void event_viewer_monitor::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape)
        close();
}
