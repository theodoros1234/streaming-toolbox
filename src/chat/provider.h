#ifndef CHAT_PROVIDER_H
#define CHAT_PROVIDER_H

#include "queue.h"
#include "../common/deregistration_interface.h"
#include "channel.h"
#include "../logging/logging.h"
#include <vector>
#include <map>
#include <mutex>

namespace strtb::chat {

struct provider_info {
    std::string id, name;
    int channel_count;
    std::vector<channel_info> channels;
};

class provider : common::deregistration_interface<channel*> {
private:
    logging::source log;
    class queue *queue;
    deregistration_interface<provider*> *deregister_provider;
    std::string id, name;
    std::map<std::string, channel*> channels;
    std::mutex lock;
protected:
    friend class system;
    void abandon();
public:
    provider(std::string id, std::string name, class queue *queue, deregistration_interface<provider*> *deregister_provider);
    ~provider();
    std::string get_id();
    std::string get_name();
    provider_info get_info();
    channel* register_channel(std::string id, std::string name);
    void deregister(channel *object);
};

}

#endif // CHAT_PROVIDER_H
