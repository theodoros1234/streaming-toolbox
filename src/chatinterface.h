#ifndef CHATINTERFACE_H
#define CHATINTERFACE_H

#include <string>
#include <vector>
#include "chatprovider.h"
#include "chatsubscription.h"

struct ChatInterfaceChannelInfo {
    int provider_count;
    int channel_count;
    std::vector<ChatProviderInfo> providers;
};

class ChatInterface {
protected:
    virtual ~ChatInterface() = default;
public:
    virtual ChatInterfaceChannelInfo getChannelInfo() = 0;
    virtual ChatProvider* registerProvider(std::string id, std::string name) = 0;
    virtual ChatSubscription* subscribe(std::string provider_id, std::string channel_id) = 0;
};

#endif // CHATINTERFACE_H
