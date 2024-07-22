#ifndef CHATMESSAGE_H
#define CHATMESSAGE_H

#include <string>
#include <vector>
#include <map>

struct ChatMessage {
    std::string provider_id, provider_name, channel_id, channel_name, user_id, user_name, user_color, message;
    bool is_mod=false, is_broadcaster=false, is_paid_member=false;
    long long int timestamp=0;
    std::map<std::string, std::string> more_metadata;
};

#endif // CHATMESSAGE_H
