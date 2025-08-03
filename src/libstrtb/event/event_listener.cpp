#include "event_listener.h"
#include "system.h"
#include "item.h"
#include "../logging/logging.h"
#include "../common/strescape.h"
#include <cassert>

using namespace strtb::event;

static strtb::logging::source log("Event Listener", false);

event_listener_base::event_listener_base(const std::string& name) : _name(name) {}

void event_listener_base::set_name(const std::string& name) {
    if (_post_first_sub)
        throw event_exception("event listener can only set its name before subscribing");

    _name = name;
}

uint64_t event_listener_base::_system_subscribe(const item_path& event_source, const json::value* param) {
    return system_ptr->event_listener_subscribe(*this, event_source, param);
}

void event_listener_base::_system_unsubscribe(uint64_t subscription_id) {
    system_ptr->event_listener_unsubscribe(subscription_id);
}

void event_listener_base::_system_unsubscribe(const std::set<uint64_t>& subscription_ids) {
    system_ptr->event_listener_unsubscribe(subscription_ids);
}

uint64_t event_listener_base::subscribe(const item_path& event_source) {
    return _subscribe(event_source, (const json::value*) nullptr);
}

uint64_t event_listener_base::subscribe(const item_path& event_source, bool param) {
    json::value_bool p(param);
    return _subscribe(event_source, &p);
}

uint64_t event_listener_base::subscribe(const item_path& event_source, int param) {
    json::value_int p(param);
    return _subscribe(event_source, &p);
}

uint64_t event_listener_base::subscribe(const item_path& event_source, long param) {
    json::value_int p(param);
    return _subscribe(event_source, &p);
}

uint64_t event_listener_base::subscribe(const item_path& event_source, long long param) {
    json::value_int p(param);
    return _subscribe(event_source, &p);
}

uint64_t event_listener_base::subscribe(const item_path& event_source, unsigned int param) {
    json::value_int p(param);
    return _subscribe(event_source, &p);
}

uint64_t event_listener_base::subscribe(const item_path& event_source, unsigned long param) {
    json::value_int p(param);
    return _subscribe(event_source, &p);
}

uint64_t event_listener_base::subscribe(const item_path& event_source, unsigned long long param) {
    json::value_int p(param);
    return _subscribe(event_source, &p);
}

uint64_t event_listener_base::subscribe(const item_path& event_source, const char* param) {
    json::value_string p(param);
    return _subscribe(event_source, &p);
}

uint64_t event_listener_base::subscribe(const item_path& event_source, const std::string& param) {
    json::value_string p(param);
    return _subscribe(event_source, &p);
}

uint64_t event_listener_base::subscribe(const item_path& event_source, const json::value* param) {
    return _subscribe(event_source, param);
}

event_listener_queued::~event_listener_queued() {
    if (_active)
        log.warning({"Destroying queued listener ", common::string_escape(_name), " while it's still active"});

    shutdown();
}

event_listener_queued::event_listener_queued(const std::string& name) : event_listener_base(name) {}

uint64_t event_listener_queued::_subscribe(const item_path& event_source, const json::value *param) {
    _post_first_sub = true;

    uint64_t new_sub = _system_subscribe(event_source, param);

    try {
        std::lock_guard<std::mutex> guard(_lock);
        if (_subs.insert(new_sub).second == false) {
            log.put(logging::ERROR, {"Failed to subscribe to event source due to internal error: "
                                     "duplicate subscription resource id in event listener"});
            throw internal_error("duplicate subscription resource id in event listener");
        }
    } catch (...) {
        _system_unsubscribe(new_sub);
        throw;
    }

    return new_sub;
}

void event_listener_queued::unsubscribe(uint64_t subscription_id) {
    {
        std::lock_guard<std::mutex> guard(_lock);
        if (_subs.erase(subscription_id) == 0)
            throw not_found("subscription doesn't exist or doesn't belong to this event listener");
    }

    _system_unsubscribe(subscription_id);
}

void event_listener_queued::shutdown() {
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
    _system_unsubscribe(old_subs);
}

void event_listener_queued::start() {
    std::lock_guard<std::mutex> guard(_lock);
    _queue.clear();
    _active = true;
}

event_holder event_listener_queued::listen() {
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

void event_listener_queued::listen(std::vector<event_holder>& destination) {
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

void event_listener_queued::push_event(uint64_t sub_id, const json::value* event) {
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
