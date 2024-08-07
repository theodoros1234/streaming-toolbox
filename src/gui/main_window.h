#ifndef GUI_MAIN_WINDOW_H
#define GUI_MAIN_WINDOW_H

#include "../plugins/list.h"
#include "chat_tab.h"
#include "../chat/interface.h"

#include <QMainWindow>
#include <QStringListModel>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

namespace gui {

class main_window : public QMainWindow {
    Q_OBJECT

public:
    main_window(chat::interface *chat_if, QWidget *parent = nullptr);
    ~main_window();
    void get_plugins(plugins::list *plugin_list);

private:
    Ui::MainWindow *ui;
    plugins::list *plugin_list;
    QWidget empty_plugin_settings_page;
    class chat_tab chat_tab;


public slots:
    void select_plugin(int index);
};

}

#endif // GUI_MAIN_WINDOW_H
