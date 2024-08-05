#ifndef GUI_MAINWINDOW_H
#define GUI_MAINWINDOW_H

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

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void getPlugins(plugins::PluginList *plugin_list);

private:
    Ui::MainWindow *ui;
    plugins::PluginList *plugin_list;
    QWidget empty_plugin_settings_page;


public slots:
    void selectPlugin(int index);
};

}

#endif // GUI_MAINWINDOW_H
