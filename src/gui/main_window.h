#ifndef STRTB_GUI_MAIN_WINDOW_H
#define STRTB_GUI_MAIN_WINDOW_H

#include "../plugins/list.h"
#include "../chat/interface.h"
#include "../config/interface.h"
#include "../logging/logging.h"
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
    main_window(plugins::list *plugin_list, chat::interface *chat_if, config::interface *config_if, QWidget *parent = nullptr);
    ~main_window();

protected:
    virtual void resizeEvent(QResizeEvent *event);

private:
    Ui::MainWindow *ui;
    class plugin_tab plugin_tab;
    class chat_tab chat_tab;
    config::interface *config_if;
    bool is_config_loaded = false;
    logging::source log;
};

}

#endif // STRTB_GUI_MAIN_WINDOW_H
