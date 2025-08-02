#include "event_listener.h"
#include "system.h"
#include "item.h"
#include "../logging/logging.h"
#include "../common/strescape.h"
#include <cassert>

using namespace strtb::event;

static strtb::logging::source log("Event Listener", false);

event_listener::event_listener(const std::string& name) : _name(name) {}

event_listener::~event_listener() {
    if (_active)
        log.warning({"Destroying listener ", common::string_escape(_name), " while it's still active"});

    shutdown();
}

void event_listener::set_name(const std::string& name) {
    if (_post_first_sub)
        throw event_exception("event listener can only set its name before subscribing");

    _name = name;
}

uint64_t event_listener::subscribe(const item_path& event_source, const json::value *param) {
    _post_first_sub = true;

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

void event_listener::start() {
    std::lock_guard<std::mutex> guard(_lock);
    _queue.clear();
    _active = true;
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

void event_listener::push_event(uint64_t sub_id, const json::value* event) {
    std::lock_guard<std::mutex> guard(_lock);

    if (!_active)
        return;

    event_holder new_event = {
        .sub_id = sub_id,
        .event = json::holder(event)
    };
    _queue.push_back(std::move(new_event));
    _cv.notify_one();
}
