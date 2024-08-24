#ifndef CHAT_SYSTEM_H
#define CHAT_SYSTEM_H

#include "interface.h"
#include "queue.h"
#include "provider.h"
#include "subscription.h"
#include "../common/deregistration_interface.h"
#include "../logging/logging.h"
#include <map>
#include <thread>

namespace strtb::chat {

class system : public interface, common::deregistration_interface<provider*>, common::deregistration_interface<subscription*> {
private:
    // Types for subscription map
    typedef std::map<subscription*, queue*> sub_map_sublist;
    typedef std::map<std::string, sub_map_sublist*> sub_map_channels;
    typedef std::map<std::string, sub_map_channels*> sub_map_providers;

    logging::source log;
    queue *incoming;
    std::thread *incoming_thread;
    std::map<std::string, provider*> providers;
    static void incoming_handler(system *target);
    std::mutex provider_lock, subscription_lock;
    // map [provider_id] [channel_id] [ptr to sub] = sub's queue
    // provider_id == "" or channel_id == "" means subscribed to all providers/channels
    // ptr to sub is only used when deregistering a sub, otherwise the inner-most map is fully iterated through
    sub_map_providers subscriptions;
public:
    system();
    ~system();
    interface_channel_info get_channel_info();
    provider* register_provider(std::string id, std::string name);
    subscription* subscribe(std::string provider_id, std::string channel_id);
    void deregister(provider* object);
    void deregister(subscription* object);
};

}

#endif // CHAT_SYSTEM_H
