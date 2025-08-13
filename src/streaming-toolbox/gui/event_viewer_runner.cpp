#include "event_viewer_runner.h"
#include "ui_event_viewer_runner.h"
#include <QGridLayout>
#include <QCheckBox>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QSpacerItem>
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
        type = new QLabel(QString::fromStdString(strtb::json::type_to_string(def->type)));

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

static void make_params_grid_object(QWidget* parent, const std::vector<strtb::event::param_definition>& params) {
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
}

event_viewer_runner::event_viewer_runner(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::event_viewer_runner)
    , _requester("GUI: Action Runner") {
    ui->setupUi(this);
}

event_viewer_runner::~event_viewer_runner() {
    delete ui;
}

void event_viewer_runner::show_with_item(const event::item_info& info, const event::item_path& path) {
    // TODO: Cancel old action and clear UI
    _requester.cancel();
    // TODO: response receiver should ignore the response if action isn't running,
    //       to prevent a race condition with the previous cancelled action
    ui->response_view->clear();
    ui->group_response->setHidden(true);
    ui->group_params->setVisible(true);

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

    if (_param_definition.empty()) {
        QVBoxLayout* param_layout = new QVBoxLayout(ui->params_scroll->widget());
        QLabel* label = new QLabel("This action takes no parameters");
        param_layout->addWidget(label);
        param_layout->addStretch();
    } else {
        make_params_grid_object(ui->params_scroll->widget(), _param_definition);
    }

    show();
    activateWindow();
    raise();
}

void event_viewer_runner::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape)
        close();
}
