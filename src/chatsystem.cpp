#include "chatsystem.h"
#include <thread>
#include <vector>
#include <iostream>

ChatSystem::ChatSystem() {
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
        for (auto &msg:messages)
            std::cout << msg.user_name << ": " << msg.message << std::endl;
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
        std::cout << "Warning: Deleting chat system with provider " << p_itr.second->getId() << " still in it. This could lead to memory leaks or a crash!" << std::endl;
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
    std::cout << "Registering new provider: " << id << std::endl;
    std::lock_guard<std::mutex> guard(this->provider_lock);
    // Abort if provider id is already used
    if (this->providers.find(id) != this->providers.end())
        return nullptr;
    // Register new provider and return it
    ChatProvider* provider = new ChatProvider(id, name, this->incoming, this);
    this->providers[id] = provider;
    return provider;
}

void ChatSystem::deregister(ChatProvider* object) {
    std::string id = object->getId();
    std::cout << "Deregistering provider: " << id << std::endl;
    std::lock_guard<std::mutex> guard(this->provider_lock);
    auto itr = this->providers.find(id);
    // Make sure the provider was actually registered
    if (itr == this->providers.end())
        std::cout << "Warning: Deregistering a provider that wasn't registered: " << id << std::endl;
    else
        this->providers.erase(itr);
}
