#include "chatprovider.h"

ChatProvider::ChatProvider(std::string id, std::string name, ChatQueue *queue, DeregistrationInterface<ChatProvider*> *deregister)
    : log("Chat Provider: " + id), queue(queue), deregister_provider(deregister), id(id), name(name) {}

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
    this->log.put(logging::INFO, {"Registering new channel: ", id});
    ChatChannel* channel;
    // Don't allow a blank id
    if (id.size() == 0) {
        this->log.put(logging::ERROR, {"Channel registration: Channel ID can't be blank"});
        throw std::invalid_argument("Channel ID can't be blank");
    }
    // Make sure the channel doesn't already exist
    if (this->channels.find(id) != this->channels.end()) {
        this->log.put(logging::ERROR, {"Channel registration: Channel '", id, "' already exists"});
        throw std::runtime_error("Channel already exists");
    }
    // Reguster new channel and return it
    channel = new ChatChannel(this->id, this->name, id, name, this->queue, this);
    this->channels[id] = channel;
    return channel;
}

void ChatProvider::deregister(ChatChannel *object) {
    std::string id = object->getId();
    this->log.put(logging::INFO, {"Deregistering channel: ", id});
    std::lock_guard<std::mutex> guard(this->lock);
    auto itr = this->channels.find(id);
    // Make sure the channel was actually registered
    if (itr == this->channels.end())
        this->log.put(logging::WARNING, {"Deregistering a channel that wasn't registered: ", object->getProviderId(), ":", object->getId()});
    else
        this->channels.erase(itr);
}

ChatProvider::~ChatProvider() {
    std::lock_guard<std::mutex> guard(this->lock);
    // Deregister from chat interface
    this->deregister_provider->deregister(this);
    // Check for channels that have been abandoned
    for (auto c_itr:this->channels)
        this->log.put(logging::WARNING, {"Deleting self with channel '", c_itr.second->getId(), "' still in it. This could lead to memory leaks or a crash!"});
}
