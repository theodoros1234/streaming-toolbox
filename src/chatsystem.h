#ifndef CHATSYSTEM_H
#define CHATSYSTEM_H

#include "chatinterface.h"
#include "chatqueue.h"
#include "chatprovider.h"
#include "chatchannel.h"
//#include "chatsubscription.h"
#include "deregistrationinterface.h"
#include <map>
#include <thread>

class ChatSystem : ChatInterface, DeregistrationInterface<ChatProvider*> {
private:
    ChatQueue *incoming;
    std::thread *incoming_thread;
    std::map<std::string, ChatProvider*> providers;
    static void incomingHandler(ChatSystem *target);
    std::mutex provider_lock;
public:
    ChatSystem();
    ~ChatSystem();
    virtual ChatInterfaceChannelInfo getChannelInfo();
    virtual ChatProvider* registerProvider(std::string id, std::string name);
    //virtual ChatSubscription* subscribe(std::string provider_id, std::string channel_id);
    virtual void deregister(ChatProvider* object);
};

#endif // CHATSYSTEM_H
