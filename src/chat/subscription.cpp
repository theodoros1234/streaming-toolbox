#include "subscription.h"

using namespace chat;

ChatSubscription::ChatSubscription(std::string provider_id, std::string channel_id,
                                   ChatQueue *queue, common::DeregistrationInterface<ChatSubscription*> *deregister)
    : log("Chat Subscription: " + provider_id + ":" + channel_id), provider_id(provider_id), channel_id(channel_id), queue(queue), deregister(deregister) {}

std::string ChatSubscription::getProviderId() {
    return this->provider_id;
}

std::string ChatSubscription::getChannelId() {
    return this->channel_id;
}

std::vector<ChatMessage> ChatSubscription::pull() {
    {
        std::lock_guard<std::mutex> guard(this->lock);
        if (this->subscribed)
            // Lock queue, so it doesn't get deleted (by unsubscribing) while we waiting on it
            this->queue->blockDeletion();
        else
            // Return empty vector if we've unsubscribed
            return std::vector<ChatMessage>();
    }
    // Get (or wait for) messages
    std::vector<ChatMessage> response = this->queue->pull();
    // Unlock the queue, now that we've finished with it, so unsubscribing is possible again
    this->queue->allowDeletion();
    return response;
}

void ChatSubscription::unsubscribe() {
    std::lock_guard guard(this->lock);
    // Mark as unsubscribed and deregister from chat system (unless abandoned)
    this->subscribed = false;
    if (this->deregister)
        this->deregister->deregister(this);
}

void ChatSubscription::abandon() {
    this->log.put(logging::WARNING, {"Abandoned by parent"});
    std::lock_guard guard(this->lock);
    // Our parent has abandoned us, so we shouldn't do any more actions that communicate with the parent
    this->subscribed = false;
    this->deregister = nullptr;
}
