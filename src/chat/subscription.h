#ifndef CHAT_SUBSCRIPTION_H
#define CHAT_SUBSCRIPTION_H

#include <string>
#include <vector>
#include <mutex>
#include "queue.h"
#include "../common/deregistration_interface.h"
#include "../logging/logging.h"

namespace strtb::chat {

class subscription {
private:
    logging::source log;
    std::string provider_id, channel_id;
    class queue *queue;
    bool subscribed = true;
    std::mutex lock;
    common::deregistration_interface<subscription*> *deregister;
protected:
    friend class system;
    void abandon();
public:
    subscription(std::string provider_id, std::string channel_id,
                     class queue *queue, common::deregistration_interface<subscription*> *deregister);
    ~subscription() = default;
    std::string get_provider_id();
    std::string get_channel_id();
    std::vector<message> pull();
    void unsubscribe();
};

}

#endif // CHAT_SUBSCRIPTION_H
