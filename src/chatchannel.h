#ifndef CHATCHANNEL_H
#define CHATCHANNEL_H

#include "chatqueue.h"
#include "deregistrationinterface.h"
#include "logging.h"
#include <vector>
#include <mutex>

struct ChatChannelInfo {
    std::string id, name;
};

class ChatChannel {
private:
    logging::LogSource log;
    std::mutex lock;
    ChatQueue *queue;
    DeregistrationInterface<ChatChannel*> *deregister;
    std::string provider_id, provider_name, channel_id, channel_name;
protected:
    friend class ChatProvider;
    void abandon();
public:
    ChatChannel(std::string provider_id, std::string provider_name,
                std::string channel_id, std::string channel_name,
                ChatQueue *queue, DeregistrationInterface<ChatChannel*> *deregister);
    ~ChatChannel();
    std::string getId();
    std::string getName();
    std::string getProviderId();
    std::string getProviderName();
    ChatChannelInfo getInfo();
    void push(ChatMessage &message);
    void push(std::vector<ChatMessage> &messages);
};

#endif // CHATCHANNEL_H
