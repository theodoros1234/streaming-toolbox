#ifndef STRTB_CHAT_CHANNEL_H
#define STRTB_CHAT_CHANNEL_H

#include "queue.h"
#include "../common/deregistration_interface.h"
#include "../logging/logging.h"
#include <vector>
#include <mutex>

namespace strtb::chat {

struct channel_info {
    std::string id, name;
};

class channel {
private:
    logging::source log;
    std::mutex lock;
    class queue *queue;
    common::deregistration_interface<channel*> *deregister;
    std::string provider_id, provider_name, channel_id, channel_name;
protected:
    friend class provider;
    void abandon();
public:
    channel(std::string provider_id, std::string provider_name,
                std::string channel_id, std::string channel_name,
                class queue *queue, common::deregistration_interface<channel*> *deregister);
    ~channel();
    std::string get_id();
    std::string get_name();
    std::string get_provider_id();
    std::string get_provider_name();
    channel_info get_info();
    void push(message &message);
    void push(std::vector<message> &messages);
};

}

#endif // STRTB_CHAT_CHANNEL_H
