#ifndef STRTB_GUI_MAIN_WINDOW_H
#define STRTB_GUI_MAIN_WINDOW_H

#include "../plugins/list.h"
#include "../libstrtb/logging/logging.h"
#include "chat_tab.h"
#include "plugin_tab.h"
#include "event_viewer.h"

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
    main_window(plugins::list *plugin_list, QWidget *parent = nullptr);
    ~main_window();

protected:
    virtual void closeEvent(QCloseEvent *event);

private:
    Ui::MainWindow *ui;
    plugin_tab _plugin_tab;
    chat_tab _chat_tab;
    event_viewer _event_viewer;
    bool is_config_loaded = false;
    logging::source log;
};

}

#endif // STRTB_GUI_MAIN_WINDOW_H
