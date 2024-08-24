#ifndef CHAT_INTERFACE_H
#define CHAT_INTERFACE_H

#include <string>
#include <vector>
#include "provider.h"
#include "subscription.h"

namespace strtb::chat {

struct interface_channel_info {
    int provider_count;
    int channel_count;
    std::vector<provider_info> providers;
};

class interface {
protected:
    virtual ~interface() = default;
public:
    virtual interface_channel_info get_channel_info() = 0;
    virtual provider* register_provider(std::string id, std::string name) = 0;
    virtual subscription* subscribe(std::string provider_id, std::string channel_id) = 0;
};

}

#endif // CHAT_INTERFACE_H
