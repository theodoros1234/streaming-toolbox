#include "chatsystem.h"
#include <thread>
#include <vector>

ChatSystem::ChatSystem() : log("Chat System") {
    // Start incoming message thread
    this->incoming = new ChatQueue();
    this->incoming->blockDeletion();
    this->incoming_thread = new std::thread(this->incomingHandler, this);
}

void ChatSystem::incomingHandler(ChatSystem *target) {
    std::vector<ChatMessage> messages;
    // Keep getting messages until the queue is deleted
    do {
        // Wait for messages
        messages = target->incoming->pull();
        // Relay messages to subscribers
        std::lock_guard<std::mutex> guard(target->subscription_lock);
        for (auto &msg:messages) {
            {
                // Find wanted provider
                auto provider = target->subscriptions.find(msg.provider_id);
                if (provider != target->subscriptions.end()) {
                    {
                        // Find wanted channel
                        auto channel = provider->second->find(msg.channel_id);
                        if (channel != provider->second->end()) {
                            for (auto sub:*channel->second) {
                                // Push to all subscribers of this channel
                                sub.second->push(msg);
                            }
                        }
                    }
                    {
                        // Also push to channel-agnostic subscribers
                        auto channel_all = provider->second->find("");
                        if (channel_all != provider->second->end()) {
                            for (auto sub:*channel_all->second) {
                                // Push to all channel-agnostic subscribers of this provider
                                sub.second->push(msg);
                            }
                        }
                    }
                }
            }
            {
                // Also push to provider-agnostic subscribers
                auto provider_all = target->subscriptions.find("");
                if (provider_all != target->subscriptions.end()) {
                    {
                        // Find wanted channel
                        auto channel = provider_all->second->find(msg.channel_id);
                        if (channel != provider_all->second->end()) {
                            for (auto sub:*channel->second) {
                                // Push to all subscribers of this channel
                                sub.second->push(msg);
                            }
                        }
                    }
                    {
                        // Also push to channel-agnostic subscribers
                        auto channel_all = provider_all->second->find("");
                        if (channel_all != provider_all->second->end()) {
                            for (auto sub:*channel_all->second) {
                                // Push to all channel-agnostic subscribers of this provider
                                sub.second->push(msg);
                            }
                        }
                    }
                }
            }
        }
    } while (!messages.empty());
    // Allow the queue to be deleted when we finish
    target->incoming->allowDeletion();
}

ChatSystem::~ChatSystem() {
    // Stop queue and wait for it to be deleted
    delete this->incoming;
    this->incoming_thread->join();
    delete this->incoming_thread;

    // Check for providers that have been abandoned
    std::lock_guard<std::mutex> guard(provider_lock);
    for (auto p_itr:this->providers)
        this->log.put(logging::WARNING, {"Deleting self with provider ", p_itr.second->getId(), " still in it. This could lead to memory leaks or a crash!"});
}

ChatInterfaceChannelInfo ChatSystem::getChannelInfo() {
    ChatInterfaceChannelInfo info;
    info.provider_count = this->providers.size();
    info.channel_count = 0;
    // Get info from all providers
    for (auto p_itr:this->providers) {
        ChatProviderInfo p_info = p_itr.second->getInfo();
        info.providers.push_back(p_info);
        // Also count total amount of channels
        info.channel_count += p_info.channel_count;
    }
    return info;
}

ChatProvider* ChatSystem::registerProvider(std::string id, std::string name) {
    this->log.put(logging::DEBUG, {"Registering new provider: ", id});
    std::lock_guard<std::mutex> guard(this->provider_lock);
    // Don't allow a blank id
    if (id.size() == 0) {
        this->log.put(logging::ERROR, {"Provider registration: Provider ID can't be blank"});
        throw std::invalid_argument("Provider ID can't be blank");
    }
    // Make sure the provider doesn't already exist
    if (this->providers.find(id) != this->providers.end()) {
        this->log.put(logging::ERROR, {"Provider registration: Provider already exists"});
        throw std::runtime_error("Provider already exists");
    }
    // Register new provider and return it
    ChatProvider* provider = new ChatProvider(id, name, this->incoming, this);
    this->providers[id] = provider;
    return provider;
}

void ChatSystem::deregister(ChatProvider* object) {
    std::string id = object->getId();
    this->log.put(logging::DEBUG, {"Deregistering provider: ", id});
    std::lock_guard<std::mutex> guard(this->provider_lock);
    auto itr = this->providers.find(id);
    // Make sure the provider was actually registered
    if (itr == this->providers.end())
        this->log.put(logging::WARNING, {"Deregistering a provider that wasn't registered: ", id});
    else
        this->providers.erase(itr);
}

ChatSubscription* ChatSystem::subscribe(std::string provider_id, std::string channel_id) {
    // Friendlier message when subscribing to any provider or any channel (which are empty ID strings)
    std::string provider_id_log = provider_id.empty() ? "(any)" : provider_id;
    std::string channel_id_log = channel_id.empty() ? "(any)" : channel_id;
    this->log.put(logging::DEBUG, {"Subscribing to ", provider_id_log, ":", channel_id_log});
    // Keep track of newly created stuff, so we can backtrack on errors
    sub_map_channels *new_provider = nullptr;
    sub_map_sublist *new_channel = nullptr;
    ChatQueue *queue = nullptr;
    ChatSubscription *sub = nullptr;
    try {
        std::lock_guard<std::mutex> guard(this->subscription_lock);
        // Make sure provider exists in subscription map
        auto provider = this->subscriptions.emplace(provider_id, nullptr);
        if (provider.second) {
            // Create it if it doesn't
            new_provider = new sub_map_channels;
            provider.first->second = new_provider;
        }
        // Make sure channel exists in subscription map
        auto channel = provider.first->second->emplace(channel_id, nullptr);
        if (channel.second) {
            // Create it if it doesn't
            new_channel = new sub_map_sublist;
            channel.first->second = new_channel;
        }
        // Create channel and its message queue
        queue = new ChatQueue();
        sub = new ChatSubscription(provider_id, channel_id, queue, this);
        // Put the sub in the submap
        channel.first->second->emplace(sub, queue);
        return sub;
    } catch (std::exception& e) {
        // On exceptions, delete any new objects (to avoid memory leaks) and pass on the exception
        this->log.put(logging::ERROR, {"Couldn't subscribe to ", provider_id_log, ":", channel_id_log, " due to exception: ", e.what()});
        if (sub)
            delete sub;
        if (queue)
            delete queue;
        if (new_channel)
            delete new_channel;
        if (new_provider)
            delete new_provider;
        throw e;
    }
}

void ChatSystem::deregister(ChatSubscription* object) {
    std::string provider_id = object->getProviderId();
    std::string channel_id = object->getChannelId();
    // Friendlier message when subscribing to any provider or any channel (which are empty ID strings)
    std::string provider_id_log = provider_id.empty() ? "(any)" : provider_id;
    std::string channel_id_log = channel_id.empty() ? "(any)" : channel_id;
    this->log.put(logging::DEBUG, {"Unsubscribing from ", provider_id_log, ":", channel_id_log});
    std::lock_guard<std::mutex> guard(this->subscription_lock);
    // Make sure subscription actually exists
    auto provider = this->subscriptions.find(provider_id);
    if (provider != this->subscriptions.end()) {
        auto channel = provider->second->find(channel_id);
        if (channel != provider->second->end()) {
            auto sub_instance = channel->second->find(object);
            if (sub_instance != channel->second->end()) {
                // Everything exists, and we can deregister the subscription properly and delete its queue
                delete sub_instance->second;
                channel->second->erase(sub_instance);
                // Also delete any map branches that are now empty
                if (channel->second->size() == 0) {
                    delete channel->second;
                    provider->second->erase(channel);
                }
                if (provider->second->size() == 0) {
                    delete provider->second;
                    this->subscriptions.erase(provider);
                }
                return;
            }
        }
    }
    // Something in the map structure doesn't exist
    this->log.put(logging::WARNING, {"Deregistering subscription that isn't registered: ", provider_id_log, ":", channel_id_log, "@", object});
}
