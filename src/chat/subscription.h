#ifndef CHAT_SUBSCRIPTION_H
#define CHAT_SUBSCRIPTION_H

#include <string>
#include <vector>
#include <mutex>
#include "queue.h"
#include "../deregistrationinterface.h"
#include "../logging/logging.h"

class ChatSubscription {
private:
    logging::LogSource log;
    std::string provider_id, channel_id;
    ChatQueue *queue;
    bool subscribed = true;
    std::mutex lock;
    DeregistrationInterface<ChatSubscription*> *deregister;
protected:
    friend class ChatSystem;
    void abandon();
public:
    ChatSubscription(std::string provider_id, std::string channel_id,
                     ChatQueue *queue, DeregistrationInterface<ChatSubscription*> *deregister);
    ~ChatSubscription() = default;
    std::string getProviderId();
    std::string getChannelId();
    std::vector<ChatMessage> pull();
    void unsubscribe();
};

#endif // CHAT_SUBSCRIPTION_H
