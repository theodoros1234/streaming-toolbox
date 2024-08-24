#ifndef GUI_MAIN_WINDOW_H
#define GUI_MAIN_WINDOW_H

#include "../plugins/list.h"
#include "../chat/interface.h"
#include "chat_tab.h"
#include "plugin_tab.h"

#include <QMainWindow>
#include <QStringListModel>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

namespace strtb::gui {

class main_window : public QMainWindow {
    Q_OBJECT

public:
    main_window(plugins::list *plugin_list, chat::interface *chat_if, QWidget *parent = nullptr);
    ~main_window();

private:
    Ui::MainWindow *ui;
    class plugin_tab plugin_tab;
    class chat_tab chat_tab;

};

}

#endif // GUI_MAIN_WINDOW_H
