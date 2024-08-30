#ifndef STRTB_GUI_CHAT_TAB_H
#define STRTB_GUI_CHAT_TAB_H

#include "../chat/interface.h"
#include "../chat/subscription.h"
#include "../logging/logging.h"
#include "chat_subscription_thread.h"

#include <QWidget>
#include <QScrollBar>

namespace Ui {
class chat_tab;
}

namespace strtb::gui {

class chat_tab : public QWidget {
    Q_OBJECT

public:
    explicit chat_tab(chat::interface *chat_if, QWidget *parent = nullptr);
    ~chat_tab();
private:
    logging::source log;
    Ui::chat_tab *ui;
    chat::subscription *chat_sub = nullptr;
    bool scrolled_to_bottom = true;
    QScrollBar *chat_view_scrollbar = nullptr;
    chat_subscription_thread *chat_sub_thread = nullptr;

private slots:
    void view_scrolled(int value);
    void view_scroll_height_changed(int min, int max);
    void scroll_to_bottom();
    void message_received(std::vector<QString> messages);

};

}

#endif // STRTB_GUI_CHAT_TAB_H
