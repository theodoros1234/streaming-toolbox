#ifndef EVENT_VIEWER_H
#define EVENT_VIEWER_H

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
    };

    Ui::event_viewer *ui;

public slots:
    void populate();
    void show_info();
};

}

#endif // EVENT_VIEWER_H
