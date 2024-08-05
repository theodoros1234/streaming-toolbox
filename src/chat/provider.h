#ifndef CHAT_PROVIDER_H
#define CHAT_PROVIDER_H

#include "queue.h"
#include "../deregistrationinterface.h"
#include "channel.h"
#include "../logging/logging.h"
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

#endif // CHAT_PROVIDER_H
