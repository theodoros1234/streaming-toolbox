#include "chatchannel.h"

ChatChannel::ChatChannel(std::string provider_id, std::string provider_name,
                         std::string channel_id, std::string channel_name,
                         ChatQueue *queue, DeregistrationInterface<ChatChannel*> *deregister)
    : provider_id(provider_id), provider_name(provider_name), channel_id(channel_id), channel_name(channel_name),
    queue(queue), deregister(deregister) {}

std::string ChatChannel::getId() {
    return this->channel_id;
}

std::string ChatChannel::getName() {
    return this->channel_name;
}

std::string ChatChannel::getProviderId() {
    return this->provider_id;
}

std::string ChatChannel::getProviderName() {
    return this->provider_name;
}

ChatChannelInfo ChatChannel::getInfo() {
    return {.id=this->channel_id, .name=this->channel_name};
}

void ChatChannel::push(ChatMessage &message) {
    // Add channel and provider info to messages
    message.channel_id = this->channel_id;
    message.channel_name = this->channel_name;
    message.provider_id = this->provider_id;
    message.provider_name = this->provider_name;
    // Send messages to queue
    this->queue->push(message);
}

void ChatChannel::push(std::vector<ChatMessage> &messages) {
    // Add channel and provider info to messages
    for (auto &msg:messages) {
        msg.channel_id = this->channel_id;
        msg.channel_name = this->channel_name;
        msg.provider_id = this->provider_id;
        msg.provider_name = this->provider_name;
    }
    // Send messages to queue
    this->queue->push(messages);
}

ChatChannel::~ChatChannel() {
    // Deregister ourselves from parent(s)
    this->deregister->deregister(this);
}
