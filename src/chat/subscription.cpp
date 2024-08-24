#include "subscription.h"

using namespace strtb;
using namespace strtb::chat;

subscription::subscription(std::string provider_id, std::string channel_id,
                                   class queue *queue, common::deregistration_interface<subscription*> *deregister)
    : log("Chat Subscription: " + provider_id + ":" + channel_id), provider_id(provider_id), channel_id(channel_id), queue(queue), deregister(deregister) {}

std::string subscription::get_provider_id() {
    return this->provider_id;
}

std::string subscription::get_channel_id() {
    return this->channel_id;
}

std::vector<message> subscription::pull() {
    {
        std::lock_guard<std::mutex> guard(this->lock);
        if (this->subscribed)
            // Lock queue, so it doesn't get deleted (by unsubscribing) while we waiting on it
            this->queue->block_deletion();
        else
            // Return empty vector if we've unsubscribed
            return std::vector<message>();
    }
    // Get (or wait for) messages
    std::vector<message> response = this->queue->pull();
    // Unlock the queue, now that we've finished with it, so unsubscribing is possible again
    this->queue->allow_deletion();
    return response;
}

void subscription::unsubscribe() {
    std::lock_guard guard(this->lock);
    // Mark as unsubscribed and deregister from chat system (unless abandoned)
    this->subscribed = false;
    if (this->deregister)
        this->deregister->deregister(this);
}

void subscription::abandon() {
    this->log.put(logging::WARNING, {"Abandoned by parent"});
    std::lock_guard guard(this->lock);
    // Our parent has abandoned us, so we shouldn't do any more actions that communicate with the parent
    this->subscribed = false;
    this->deregister = nullptr;
}
