#ifndef CHATPROVIDER_H
#define CHATPROVIDER_H

#include "chatqueue.h"
#include "deregistrationinterface.h"
#include "chatchannel.h"
#include <vector>
#include <map>
#include <mutex>

struct ChatProviderInfo {
    std::string id, name;
    int channel_count;
    std::vector<ChatChannelInfo> channels;
};

class ChatProvider : DeregistrationInterface<ChatChannel*> {
private:
    ChatQueue *queue;
    DeregistrationInterface<ChatProvider*> *deregister_provider;
    std::string id, name;
    std::map<std::string, ChatChannel*> channels;
    std::mutex lock;
public:
    ChatProvider(std::string id, std::string name, ChatQueue *queue, DeregistrationInterface<ChatProvider*> *deregister_provider);
    virtual ~ChatProvider();
    std::string getId();
    std::string getName();
    ChatProviderInfo getInfo();
    ChatChannel* registerChannel(std::string id, std::string name);
    virtual void deregister(ChatChannel *object);
};

#endif // CHATPROVIDER_H
