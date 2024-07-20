#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringListModel>
#include <QWidget>
#include "pluginlist.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void getPlugins(PluginList *plugin_list);

private:
    Ui::MainWindow *ui;
    PluginList *plugin_list;
    QWidget empty_plugin_settings_page;


public slots:
    void selectPlugin(int index);
};
#endif // MAINWINDOW_H
