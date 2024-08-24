#include "provider.h"

using namespace strtb;
using namespace strtb::chat;

provider::provider(std::string id, std::string name, class queue *queue, deregistration_interface<provider*> *deregister)
    : log("Chat Provider: " + id), queue(queue), deregister_provider(deregister), id(id), name(name) {}

std::string provider::get_id() {
    return this->id;
}

std::string provider::get_name() {
    return this->name;
}

provider_info provider::get_info() {
    std::lock_guard<std::mutex> guard(this->lock);
    provider_info info;
    // Get own info
    info.id = this->id;
    info.name = this->name;
    info.channel_count = this->channels.size();
    // Get channel info from all our channels
    for (auto item : channels)
        info.channels.push_back(item.second->get_info());
    return info;
}

channel* provider::register_channel(std::string id, std::string name) {
    std::lock_guard<std::mutex> guard(this->lock);
    this->log.put(logging::DEBUG, {"Registering new channel: ", id});
    channel* channel;
    // Stop if this provider was abandoned by parent
    if (!this->queue) {
        this->log.put(logging::ERROR, {"Can't register new channel when abandoned by parent"});
        throw std::runtime_error("Can't register new channel when abandoned by parent");
    }
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
    channel = new class channel(this->id, this->name, id, name, this->queue, this);
    this->channels[id] = channel;
    return channel;
}

void provider::deregister(channel *object) {
    std::string id = object->get_id();
    this->log.put(logging::DEBUG, {"Deregistering channel: ", id});
    std::lock_guard<std::mutex> guard(this->lock);
    auto itr = this->channels.find(id);
    // Make sure the channel was actually registered
    if (itr == this->channels.end())
        this->log.put(logging::WARNING, {"Deregistering a channel that wasn't registered: ", object->get_provider_id(), ":", object->get_id()});
    else
        this->channels.erase(itr);
}

void provider::abandon() {
    this->log.put(logging::WARNING, {"Abandoned by parent"});
    std::lock_guard<std::mutex> guard(this->lock);
    // Our parent has abandoned us, so we and our children shouldn't do any more actions that communicate with the parent to avoid crashes
    this->queue = nullptr;
    this->deregister_provider = nullptr;
    for (auto c_itr : this->channels)
        c_itr.second->abandon();
}

provider::~provider() {
    std::lock_guard<std::mutex> guard(this->lock);
    // Skip if abandoned by parent
    if (!this->deregister_provider)
        return;
    // Deregister from chat interface
    this->deregister_provider->deregister(this);
    // Check for channels that will be abandoned
    for (auto c_itr : this->channels)
        // Notify them that they're being abandoned, to prevent a future crash
        c_itr.second->abandon();
}
