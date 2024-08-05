#ifndef CHATPROVIDER_H
#define CHATPROVIDER_H

#include "chatqueue.h"
#include "deregistrationinterface.h"
#include "chatchannel.h"
#include "logging.h"
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
    logging::LogSource log;
    ChatQueue *queue;
    DeregistrationInterface<ChatProvider*> *deregister_provider;
    std::string id, name;
    std::map<std::string, ChatChannel*> channels;
    std::mutex lock;
protected:
    friend class ChatSystem;
    void abandon();
public:
    ChatProvider(std::string id, std::string name, ChatQueue *queue, DeregistrationInterface<ChatProvider*> *deregister_provider);
    ~ChatProvider();
    std::string getId();
    std::string getName();
    ChatProviderInfo getInfo();
    ChatChannel* registerChannel(std::string id, std::string name);
    void deregister(ChatChannel *object);
};

#endif // CHATPROVIDER_H
