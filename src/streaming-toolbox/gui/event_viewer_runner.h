#ifndef STRTB_GUI_EVENT_VIEWER_RUNNER_H
#define STRTB_GUI_EVENT_VIEWER_RUNNER_H

#include "../../libstrtb/event/action_requester.h"
#include "../../libstrtb/event/item.h"
#include <QWidget>
#include <QKeyEvent>
#include <QGridLayout>
#include <QMessageBox>
#include <vector>

namespace Ui {
class event_viewer_runner;
}

namespace strtb::gui {

class event_viewer_runner : public QWidget {
    Q_OBJECT

public:
    explicit event_viewer_runner(QWidget *parent = nullptr);
    ~event_viewer_runner();
    void show_with_item(const event::item_info& info, const event::item_path& path);

protected:
    virtual void closeEvent(QCloseEvent* event);
    virtual void keyPressEvent(QKeyEvent* event);

private:
    Ui::event_viewer_runner *ui;
    event::action_requester_qt_signal _requester;
    std::vector<event::param_definition> _param_definition;
    QGridLayout* _param_grid = nullptr;
    QMessageBox _error;     // only used when failing to parse custom JSON params

private slots:
    void run_or_cancel();
    void received_response(event::action_response response);
};

}

#endif // STRTB_GUI_EVENT_VIEWER_RUNNER_H
