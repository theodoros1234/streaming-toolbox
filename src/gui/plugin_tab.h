#ifndef PLUGIN_TAB_H
#define PLUGIN_TAB_H

#include "../plugins/list.h"

#include <QWidget>
#include <QLabel>

namespace Ui {
class plugin_tab;
}

namespace strtb::gui {

class plugin_tab : public QWidget {
    Q_OBJECT

public:
    explicit plugin_tab(plugins::list *plugin_list, QWidget *parent = nullptr);
    ~plugin_tab();

private:
    Ui::plugin_tab *ui;
    plugins::list *plugin_list;
    QLabel *placeholder_no_settings;

private slots:
    void select_plugin(int index);
};

}

#endif // PLUGIN_TAB_H
