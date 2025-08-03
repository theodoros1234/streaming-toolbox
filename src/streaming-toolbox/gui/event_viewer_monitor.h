#ifndef EVENT_VIEWER_MONITOR_H
#define EVENT_VIEWER_MONITOR_H

#include "../libstrtb/event/item.h"
#include "../libstrtb/event/event_listener.h"
#include <QWidget>
#include <QScrollBar>

namespace Ui {
class event_viewer_monitor;
}

namespace strtb::gui {

class event_viewer_monitor : public QWidget {
    Q_OBJECT

public:
    explicit event_viewer_monitor(QWidget *parent = nullptr);
    ~event_viewer_monitor();
    void show_with_item(const event::item_info& info, const event::item_path& path);

protected:
    virtual void closeEvent(QCloseEvent* event);
    virtual void keyPressEvent(QKeyEvent* event);

private:
    Ui::event_viewer_monitor *ui;
    event::param_definition _param;
    event::item_path _path;
    QScrollBar* _scrollbar;
    event::event_listener_qt_signal _listener = std::string("GUI: Event Monitor");
    uint64_t _sub_id = 0;

private slots:
    void subscribe();
    void event_received(uint64_t sub_id, json::holder event);
    void on_param_omit_checkStateChanged(Qt::CheckState state);
    void on_button_scroll_clicked();
};

}

#endif // EVENT_VIEWER_MONITOR_H
