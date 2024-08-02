#ifndef CHATSYSTEM_H
#define CHATSYSTEM_H

#include "chatinterface.h"
#include "chatqueue.h"
#include "chatprovider.h"
#include "chatsubscription.h"
#include "deregistrationinterface.h"
#include "logging.h"
#include <map>
#include <thread>

class ChatSystem : ChatInterface, DeregistrationInterface<ChatProvider*>, DeregistrationInterface<ChatSubscription*> {
private:
    // Types for subscription map
    typedef std::map<ChatSubscription*, ChatQueue*> sub_map_sublist;
    typedef std::map<std::string, sub_map_sublist*> sub_map_channels;
    typedef std::map<std::string, sub_map_channels*> sub_map_providers;

    logging::LogSource log;
    ChatQueue *incoming;
    std::thread *incoming_thread;
    std::map<std::string, ChatProvider*> providers;
    static void incomingHandler(ChatSystem *target);
    std::mutex provider_lock, subscription_lock;
    // map [provider_id] [channel_id] [ptr to sub] = sub's queue
    // provider_id == "" or channel_id == "" means subscribed to all providers/channels
    // ptr to sub is only used when deregistering a sub, otherwise the inner-most map is fully iterated through
    sub_map_providers subscriptions;
public:
    ChatSystem();
    ~ChatSystem();
    ChatInterfaceChannelInfo getChannelInfo();
    ChatProvider* registerProvider(std::string id, std::string name);
    ChatSubscription* subscribe(std::string provider_id, std::string channel_id);
    void deregister(ChatProvider* object);
    void deregister(ChatSubscription* object);
};

#endif // CHATSYSTEM_H
