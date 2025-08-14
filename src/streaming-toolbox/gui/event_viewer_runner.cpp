#include "event_viewer_runner.h"
#include "ui_event_viewer_runner.h"
#include "../../libstrtb/json/parser.h"
#include <QGridLayout>
#include <QCheckBox>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QFontDatabase>
#include <climits>

using namespace strtb::gui;

static void make_param_row(QGridLayout* layout, int row, const strtb::event::param_definition* def) {
    QCheckBox* included = nullptr;
    QLabel* name = nullptr;
    QLabel* type = nullptr;
    QWidget* value = nullptr;

    try {
        included = new QCheckBox();
        included->setChecked(def->required);
        included->setDisabled(def->required);

        name = new QLabel(QString::fromStdString(def->name));
        type = new QLabel(strtb::json::type_to_string(def->type));

        switch (def->type) {
        case strtb::json::VAL_BOOL:
            value = new QCheckBox("true/false");
            break;

        case strtb::json::VAL_INT:
            value = new QSpinBox();
            ((QSpinBox*) value)->setRange(INT_MIN, INT_MAX);
            break;

        case strtb::json::VAL_FLOAT:
            value = new QDoubleSpinBox();
            ((QDoubleSpinBox*) value)->setDecimals(4);
            break;

        case strtb::json::VAL_STRING:
            value = new QLineEdit();
            break;

        case strtb::json::VAL_ARRAY:
            value = new QLineEdit();
            ((QLineEdit*) value)->setPlaceholderText("Not implemented, type some JSON here instead.");
            break;

        case strtb::json::VAL_OBJECT:
            value = new QLineEdit();
            ((QLineEdit*) value)->setPlaceholderText("Not implemented, type some JSON here instead.");
            break;

        case strtb::json::VAL_UNDEFINED:
            value = new QLineEdit();
            ((QLineEdit*) value)->setPlaceholderText("Not implemented, type some JSON here instead.");
            break;

        default:
            break;
        }

        layout->addWidget(included, row, 0);
        included = nullptr;
        layout->addWidget(name, row, 1);
        name = nullptr;
        layout->addWidget(type, row, 2);
        type = nullptr;
        layout->addWidget(value, row, 3);
        value = nullptr;
    } catch (...) {
        if (included)
            delete included;
        if (name)
            delete name;
        if (type)
            delete type;
        if (value)
            delete value;
        throw;
    }
}

static QGridLayout* make_params_grid_object(QWidget* parent,
                                            const std::vector<strtb::event::param_definition>& params) {
    QGridLayout* grid = new QGridLayout(parent);
    QLabel* label = nullptr;  // for deletion if there's an exception

    try {
        // Headers
        label = new QLabel("**Include**");
        label->setTextFormat(Qt::MarkdownText);
        grid->addWidget(label, 0, 0);
        label = nullptr;
        label = new QLabel("**Name**");
        label->setTextFormat(Qt::MarkdownText);
        grid->addWidget(label, 0, 1);
        label = nullptr;
        label = new QLabel("**Type**");
        label->setTextFormat(Qt::MarkdownText);
        grid->addWidget(label, 0, 2);
        label = nullptr;
        label = new QLabel("**Value**");
        label->setTextFormat(Qt::MarkdownText);
        grid->addWidget(label, 0, 3);
        label = nullptr;

        // Params
        for (const auto& param : params)
            make_param_row(grid, grid->rowCount(), &param);

        // Spacing when window is bigger than needed
        QSpacerItem* spacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        grid->addItem(spacer, grid->rowCount(), 0);
    } catch (...) {
        if (label)
            delete label;
        throw;
    }

    return grid;
}

event_viewer_runner::event_viewer_runner(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::event_viewer_runner)
    , _requester("GUI: Action Runner") {
    ui->setupUi(this);
    ui->group_response->setHidden(true);

    ui->response_view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    QObject::connect(ui->button_run, &QPushButton::clicked, this, &event_viewer_runner::run_or_cancel);
    QObject::connect(&_requester.emitter, &event::action_requester_qt_signal_emitter::response_received,
                     this, &event_viewer_runner::received_response, Qt::QueuedConnection);
}

event_viewer_runner::~event_viewer_runner() {
    delete ui;
}

void event_viewer_runner::show_with_item(const event::item_info& info, const event::item_path& path) {
    if (_requester.running())
        run_or_cancel();    // will cancel
    _requester.path_set(path);

    // Info
    ui->label_name->setText(QString::fromStdString(info.display_name));
    ui->label_path->setText(QString::fromStdString(path.to_string()));

    // Params
    _param_definition = info.params;

    // Delete previous layout and all its items
    QLayout* params_old_layout = ui->params_scroll->widget()->layout();
    if (params_old_layout) {
        QLayoutItem* child;
        while ((child = params_old_layout->takeAt(0)) != nullptr) {
            delete child->widget();
            delete child;
        }
        delete params_old_layout;
    }
    _param_grid = nullptr;

    if (_param_definition.empty()) {
        QVBoxLayout* param_layout = new QVBoxLayout(ui->params_scroll->widget());
        QLabel* label = new QLabel("This action takes no parameters");
        param_layout->addWidget(label);
        param_layout->addStretch();
    } else {
        _param_grid = make_params_grid_object(ui->params_scroll->widget(), _param_definition);
    }

    show();
    activateWindow();
    raise();
}

void event_viewer_runner::run_or_cancel() {
    bool running = _requester.running();
    if (running) {  // Cancel
        _requester.cancel();
        ui->button_run->setText("&Run");
        ui->response_view->clear();
    } else {        // Run
        // Get params from UI
        _requester.params().clear();
        if (_param_grid) {
            for (size_t i=0; i<_param_definition.size(); i++) {
                event::param_definition& def = _param_definition[i];
                QLayoutItem* included = _param_grid->itemAtPosition(i+1, 0);
                if (included == nullptr)
                    break;

                if (((QCheckBox*) included->widget())->isChecked()) {
                    QLayoutItem* value = _param_grid->itemAtPosition(i+1, 3);
                    if (value == nullptr)
                        break;

                    switch (def.type) {
                    case json::VAL_BOOL:
                        _requester.params().set(def.name, ((QCheckBox*) value->widget())->isChecked());
                        break;

                    case json::VAL_INT:
                        _requester.params().set(def.name, ((QSpinBox*) value->widget())->value());
                        break;

                    case json::VAL_FLOAT:
                        _requester.params().set(def.name, ((QDoubleSpinBox*) value->widget())->value());
                        break;

                    case json::VAL_STRING:
                        _requester.params().set(def.name, ((QLineEdit*) value->widget())->text().toStdString());
                        break;

                    case json::VAL_ARRAY:
                    case json::VAL_OBJECT:
                    case json::VAL_UNDEFINED: {
                        QLineEdit* textbox = (QLineEdit*) value->widget();
                        textbox->setStyleSheet("");
                        try {
                            _requester.params().set_move(
                                def.name,
                                json::parser::from_string(textbox->text().toStdString()));
                        } catch (json::parser::invalid_json&) {
                            textbox->setStyleSheet("border-color: red;");
                            return;
                        }
                    }
                    break;

                    default:
                        break;
                    }
                }
            }
        }

        _requester.run();
        ui->button_run->setText("&Reset");
        ui->response_view->setPlainText("Running action...");
    }

    ui->group_params->setVisible(running);
    ui->group_response->setHidden(running);
}

void event_viewer_runner::received_response(event::action_response response) {
    if (!_requester.running())
        return;

    switch (response.status) {
    case event::ACTION_DONE:
        if (response.returns.empty()) {
            ui->response_view->setPlainText("Responded with a null pointer. "
                                            "This may be a bug in Streaming Toolbox.");
        } else {
            ui->response_view->setPlainText("Response: " +
                                            QString::fromStdString(response.returns.value()->write_to_string(4)));
        }
        break;

    case event::ACTION_ERROR:
        ui->response_view->setPlainText("Error: " + QString::fromStdString(response.diagnostic_info));
        break;

    default:
        ui->response_view->setPlainText("Response has invalid status code of " +
                                        QString::number(response.status));
    }
}

void event_viewer_runner::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape)
        close();
}

void event_viewer_runner::closeEvent(QCloseEvent*) {
    if (_requester.running())
        run_or_cancel();    // will cancel the current action
    _requester.path_clear();
}
