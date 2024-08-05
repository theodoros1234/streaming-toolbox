#ifndef CHAT_MESSAGE_H
#define CHAT_MESSAGE_H

#include <string>
#include <map>

namespace chat {

struct message {
    std::string provider_id, provider_name, channel_id, channel_name, user_id, user_name, user_color, message;
    bool is_mod=false, is_broadcaster=false, is_paid_member=false;
    long long int timestamp=0;
    std::map<std::string, std::string> more_metadata;
};

}

#endif // CHAT_MESSAGE_H
