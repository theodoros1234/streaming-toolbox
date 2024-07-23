#include "chatsubscription.h"

ChatSubscription::ChatSubscription(std::string provider_id, std::string channel_id,
                                   ChatQueue *queue, DeregistrationInterface<ChatSubscription*> *deregister)
    : provider_id(provider_id), channel_id(channel_id), queue(queue), deregister(deregister) {}

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
    // Mark as unsubscribed and deregister from chat system
    this->subscribed = false;
    this->deregister->deregister(this);
}
