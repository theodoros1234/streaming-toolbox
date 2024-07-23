#ifndef CHATSUBSCRIPTION_H
#define CHATSUBSCRIPTION_H

#include <string>
#include <vector>
#include <mutex>
#include "chatqueue.h"
#include "deregistrationinterface.h"

class ChatSubscription {
private:
    std::string provider_id, channel_id;
    ChatQueue *queue;
    bool subscribed = true;
    std::mutex lock;
    DeregistrationInterface<ChatSubscription*> *deregister;
public:
    ChatSubscription(std::string provider_id, std::string channel_id,
                     ChatQueue *queue, DeregistrationInterface<ChatSubscription*> *deregister);
    ~ChatSubscription() = default;
    std::string getProviderId();
    std::string getChannelId();
    std::vector<ChatMessage> pull();
    void unsubscribe();
};

#endif // CHATSUBSCRIPTION_H
