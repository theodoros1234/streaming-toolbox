#ifndef CHAT_SUBSCRIPTION_THREAD_H
#define CHAT_SUBSCRIPTION_THREAD_H

#include "../chat/subscription.h"

#include <QThread>
#include <QString>

class chat_subscription_thread : public QThread {
    Q_OBJECT

public:
    chat_subscription_thread(logging::source *log, chat::subscription *sub);
    void run() override;

private:
    logging::source *log;
    chat::subscription *sub;

signals:
    void messages_received(std::vector<QString>);
};

#endif // CHAT_SUBSCRIPTION_THREAD_H
