#ifndef EVENT_VIEWER_MONITOR_H
#define EVENT_VIEWER_MONITOR_H

#include "../libstrtb/event/item.h"
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
    // virtual void closeEvent(QCloseEvent *event);

private:
    Ui::event_viewer_monitor *ui;
    event::param_definition _param;
    event::item_path _path;
    QScrollBar* _scrollbar;

private slots:
    void subscribe();
};

}

#endif // EVENT_VIEWER_MONITOR_H
