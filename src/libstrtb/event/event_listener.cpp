#include "event_listener.h"
#include "system.h"
#include "item.h"
#include "../logging/logging.h"
#include <cassert>

using namespace strtb::event;

static strtb::logging::source log("Event Listener");

event_listener::~event_listener() {
    shutdown();
}

uint64_t event_listener::subscribe(const item_path& event_source, const json::value *param) {
    uint64_t new_sub = system_ptr->event_listener_subscribe(*this, event_source, param);

    try {
        std::lock_guard<std::mutex> guard(_lock);
        if (_subs.insert(new_sub).second == false) {
            log.put(logging::ERROR, {"Failed to subscribe to event source due to internal error: "
                                     "duplicate subscription resource id in event listener"});
            throw internal_error("duplicate subscription resource id in event listener");
        }
    } catch (...) {
        system_ptr->event_listener_unsubscribe(new_sub);
        throw;
    }

    return new_sub;
}

void event_listener::unsubscribe(uint64_t subscription_id) {
    {
        std::lock_guard<std::mutex> guard(_lock);
        if (_subs.erase(subscription_id) == 0)
            throw not_found("subscription doesn't exist or doesn't belong to this event listener");
    }

    system_ptr->event_listener_unsubscribe(subscription_id);
}

void event_listener::shutdown() {
    std::set<uint64_t> old_subs;

    {
        std::lock_guard<std::mutex> guard(_lock);

        // Get subs so we can cancel them
        old_subs = std::move(_subs);
        _subs.clear();

        // Wake up listeners
        _active = false;
        _cv.notify_all();
    }

    // Cancel old subs
    system_ptr->event_listener_unsubscribe(old_subs);
}

void event_listener::reset() {
    std::set<uint64_t> old_subs;

    {
        std::lock_guard<std::mutex> guard(_lock);

        // Get subs so we can cancel them
        old_subs = std::move(_subs);
        _subs.clear();

        // Discard any unprocessed events
        _queue.clear();

        // Event listener can now be listened to again
        _active = true;
    }

    // Cancel old subs
    system_ptr->event_listener_unsubscribe(old_subs);
}

size_t event_listener::discard() {
    std::lock_guard<std::mutex> guard(_lock);
    size_t count = _queue.size();
    _queue.clear();
    return count;
}

event_holder event_listener::listen() {
    std::unique_lock<std::mutex> guard(_lock);

    // Wait for events or for shutdown
    while (_active && _queue.empty())
        _cv.wait(guard);

    // Empty event with sub_id=0 returned on shutdown
    if (!_active)
        return event_holder();

    assert(!_queue.empty());
    event_holder event = std::move(_queue.front());
    _queue.pop_front();
    return event;
}

void event_listener::listen(std::vector<event_holder>& destination) {
    std::unique_lock<std::mutex> guard(_lock);
    destination.clear();

    // Wait for events or for shutdown
    while (_active && _queue.empty())
        _cv.wait(guard);

    // Empty vector returned on shutdown
    if (!_active)
        return;

    // Empty the queue into the destination vector
    assert(!_queue.empty());
    destination.reserve(_queue.size());
    for (auto& event : _queue)
        destination.emplace_back(std::move(event));
    _queue.clear();
}
