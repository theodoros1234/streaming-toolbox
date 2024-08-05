#ifndef GUI_MAIN_WINDOW_H
#define GUI_MAIN_WINDOW_H

#include <QMainWindow>
#include <QStringListModel>
#include <QWidget>
#include "../plugins/list.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

namespace gui {

class main_window : public QMainWindow {
    Q_OBJECT

public:
    main_window(QWidget *parent = nullptr);
    ~main_window();
    void get_plugins(plugins::list *plugin_list);

private:
    Ui::MainWindow *ui;
    plugins::list *plugin_list;
    QWidget empty_plugin_settings_page;


public slots:
    void select_plugin(int index);
};

}

#endif // GUI_MAIN_WINDOW_H
