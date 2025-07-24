#include "chat_subscription_thread.h"

using namespace strtb;
using namespace strtb::gui;

chat_subscription_thread::chat_subscription_thread(logging::source *log, chat::subscription *sub) : log(log), sub(sub) {}

void chat_subscription_thread::run() {
    this->log->put(logging::DEBUG, {"Started message receiving thread"});

    std::vector<chat::message> buffer = sub->pull();
    while (!buffer.empty()) {
        // Process incoming messages
        std::vector<QString> messages;
        messages.reserve(buffer.size());
        for (auto msg : buffer) {
            // Format message into a string
            QString msg_str = QString("<b><font color=\"");
            msg_str.append(QString(msg.user_color.c_str()).toHtmlEscaped());
            msg_str.append("\">");
            msg_str.append(QString(msg.user_name.c_str()).toHtmlEscaped());
            msg_str.append("</font></b>: ");
            msg_str.append(QString(msg.message.c_str()).toHtmlEscaped());
            messages.push_back(msg_str);
        }
        // Send processed messages upstream through signal
        emit messages_received(messages);
        // Pull next messages
        buffer = sub->pull();
    }

    this->log->put(logging::DEBUG, {"Stopping message receiving thread"});
}
