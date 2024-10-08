#include "channel.h"

using namespace strtb;
using namespace strtb::chat;

channel::channel(std::string provider_id, std::string provider_name,
                         std::string channel_id, std::string channel_name,
                         class queue *queue, common::deregistration_interface<channel*> *deregister)
    : log("Chat Channel: " + provider_id + ":" + channel_id), queue(queue), deregister(deregister),
    provider_id(provider_id), provider_name(provider_name), channel_id(channel_id), channel_name(channel_name) {}

std::string channel::get_id() {
    return this->channel_id;
}

std::string channel::get_name() {
    return this->channel_name;
}

std::string channel::get_provider_id() {
    return this->provider_id;
}

std::string channel::get_provider_name() {
    return this->provider_name;
}

channel_info channel::get_info() {
    return {.id=this->channel_id, .name=this->channel_name};
}

void channel::push(message &message) {
    // Add channel and provider info to message
    message.channel_id = this->channel_id;
    message.channel_name = this->channel_name;
    message.provider_id = this->provider_id;
    message.provider_name = this->provider_name;
    // Send message to queue
    {
        std::lock_guard<std::mutex> guard(this->lock);
        if (this->queue)
            this->queue->push(message);
        else
            this->log.put(logging::ERROR, {"Can't push new message when abandoned by parent"});
    }
}

void channel::push(std::vector<message> &messages) {
    // Add channel and provider info to messages
    for (auto &msg : messages) {
        msg.channel_id = this->channel_id;
        msg.channel_name = this->channel_name;
        msg.provider_id = this->provider_id;
        msg.provider_name = this->provider_name;
    }
    // Send messages to queue, unless abandoned
    {
        std::lock_guard<std::mutex> guard(this->lock);
        if (this->queue)
            this->queue->push(messages);
        else
            this->log.put(logging::ERROR, {"Can't push new messages when abandoned by parent"});
    }
}

void channel::abandon() {
    this->log.put(logging::WARNING, {"Abandoned by parent"});
    std::lock_guard<std::mutex> guard(this->lock);
    // Our parent has abandoned us, so we shouldn't do any more actions that communicate with the parent to avoid crashes
    this->queue = nullptr;
    this->deregister = nullptr;
}

channel::~channel() {
    std::lock_guard<std::mutex> guard(this->lock);
    // Deregister ourselves from parent, unless abandoned
    if (this->deregister)
        this->deregister->deregister(this);
}
