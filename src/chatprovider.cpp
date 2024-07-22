#include "chatprovider.h"

#include <iostream>

ChatProvider::ChatProvider(std::string id, std::string name, ChatQueue *queue, DeregistrationInterface<ChatProvider*> *deregister)
    : id(id), name(name), queue(queue), deregister_provider(deregister) {}

std::string ChatProvider::getId() {
    return this->id;
}

std::string ChatProvider::getName() {
    return this->name;
}

ChatProviderInfo ChatProvider::getInfo() {
    std::lock_guard<std::mutex> guard(this->lock);
    ChatProviderInfo info;
    // Get own info
    info.id = this->id;
    info.name = this->name;
    info.channel_count = this->channels.size();
    // Get channel info from all our channels
    for (auto item:channels)
        info.channels.push_back(item.second->getInfo());
    return info;
}

ChatChannel* ChatProvider::registerChannel(std::string id, std::string name) {
    std::lock_guard<std::mutex> guard(this->lock);
    std::cout << "Registering new channel: " << this->id << ":" << id << std::endl;
    ChatChannel* channel;
    // Make sure the channel doesn't already exist
    if (this->channels.find(id) != this->channels.end())
        return nullptr;
    // Reguster new channel and return it
    channel = new ChatChannel(this->id, this->name, id, name, this->queue, this);
    this->channels[id] = channel;
    return channel;
}

void ChatProvider::deregister(ChatChannel *object) {
    std::string id = object->getId();
    std::cout << "Deregistering channel: " << this->id << ":" << id << std::endl;
    std::lock_guard<std::mutex> guard(this->lock);
    auto itr = this->channels.find(id);
    // Make sure the channel was actually registered
    if (itr == this->channels.end())
        std::cout << "Warning: Deregistering a channel that wasn't registered: " << object->getProviderId() << ":" << object->getId() << std::endl;
    else
        this->channels.erase(itr);
}

ChatProvider::~ChatProvider() {
    std::lock_guard<std::mutex> guard(this->lock);
    // Deregister from chat interface
    this->deregister_provider->deregister(this);
    // Check for channels that have been abandoned
    for (auto c_itr:this->channels)
        std::cout << "Warning: Deleting provider '" << this->id << "' with channel '" << c_itr.second->getId() << "' still in it. This could lead to memory leaks or a crash!" << std::endl;
}
