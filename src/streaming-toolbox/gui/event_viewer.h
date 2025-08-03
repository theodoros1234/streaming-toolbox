#ifndef EVENT_VIEWER_H
#define EVENT_VIEWER_H

#include "event_viewer_monitor.h"
#include "../libstrtb/event/item.h"
#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>

namespace Ui {
class event_viewer;
}

namespace strtb::gui {

class event_viewer : public QWidget {
    Q_OBJECT

public:
    explicit event_viewer(QWidget *parent = nullptr);
    ~event_viewer();

private:
    class tree_item : public QTreeWidgetItem {
    public:
        event::item_listing event_item_info;
        void populate();
        void get_path(event::item_path& path);
        void find_item(const event::item_path& path, size_t pos);
    };

    Ui::event_viewer *ui;
    event_viewer_monitor _event_monitor_ui;

private slots:
    void populate();
    void on_item_path_copy_clicked();
    void launch_event_monitor(QTreeWidgetItem* tree_widget_item, int column);
    void on_item_tree_currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
};

}

#endif // EVENT_VIEWER_H
