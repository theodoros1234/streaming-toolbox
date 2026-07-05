#include "event_listener.h"
#include "system.h"
#include "item.h"
#include "../logging.h"
#include "../strescape.h"
#include <cassert>

using namespace strtb::event;

static strtb::logging::source log_s("Event Listener", false);

event_listener_base::event_listener_base(const std::string& name) : _name(name) {}

void event_listener_base::set_name(const std::string& name) {
    if (_post_first_sub)
        throw event_exception("event listener can only set its name before subscribing");

    _name = name;
}

// NOTE: don't forget to lock the event system's mutex from inheriting classes (done from here to prevent deadlocks)

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
        log_s.warning({"Destroying queued listener ", string_escape(_name), " while it's still active"});

    stop();
}

uint64_t event_listener_queued::_subscribe(const item_path& event_source, const json::value *param) {
    std::lock_guard<std::mutex> guard_system(system_ptr->_lock);
    std::lock_guard<std::mutex> guard(_lock);
    _post_first_sub = true;

    uint64_t new_sub = _system_subscribe(event_source, param);

    try {
        if (_subs.insert(new_sub).second == false)
            throw internal_error("duplicate subscription resource id in event listener", log_s, __FILE__, __LINE__, __func__);
    } catch (...) {
        _system_unsubscribe(new_sub);
        throw;
    }

    return new_sub;
}

void event_listener_queued::unsubscribe(uint64_t subscription_id) {
    std::lock_guard<std::mutex> guard_system(system_ptr->_lock);
    std::lock_guard<std::mutex> guard(_lock);
    if (_subs.erase(subscription_id) == 0)
        throw not_found("subscription doesn't exist or doesn't belong to this event listener");
    _system_unsubscribe(subscription_id);
}

void event_listener_queued::stop() {
    std::lock_guard<std::mutex> guard_system(system_ptr->_lock);
    std::lock_guard<std::mutex> guard(_lock);

    // Cancel old subs
    _system_unsubscribe(_subs);
    _subs.clear();
    _queue.clear();

    // Wake up listeners
    _active = false;
    _cv.notify_all();
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

event_listener_qt_signal::~event_listener_qt_signal() {
    if (!_subs.empty()) {
        log_s.warning({"Destroying Qt signal listener ", string_escape(_name), " while it still has active subscriptions"});
        stop();
    }
}

uint64_t event_listener_qt_signal::_subscribe(const item_path& event_source, const json::value* param) {
    std::lock_guard<std::mutex> guard_system(system_ptr->_lock);
    _post_first_sub = true;
    uint64_t sub_rid = _system_subscribe(event_source, param);

    try {
        if (!_subs.insert(sub_rid).second)
            throw internal_error("duplicate subscription resource id in event listener", log_s, __FILE__, __LINE__, __func__);
    } catch (...) {
        _system_unsubscribe(sub_rid);
        throw;
    }

    return sub_rid;
}

void event_listener_qt_signal::unsubscribe(uint64_t subscription_id) {
    std::lock_guard<std::mutex> guard_system(system_ptr->_lock);
    if (_subs.erase(subscription_id) == 0)
        throw not_found("subscription doesn't exist or doesn't belong to this event listener");
    _system_unsubscribe(subscription_id);
}

void event_listener_qt_signal::stop() {
    std::lock_guard<std::mutex> guard_system(system_ptr->_lock);
    _system_unsubscribe(_subs);
    _subs.clear();
}

void event_listener_qt_signal::push_event(uint64_t sub_id, const json::value* event) {
    emitter.push_event(sub_id, event);
}

void event_listener_qt_signal_emitter::push_event(uint64_t sub_id, const json::value* event) {
    emit event_received(sub_id, json::holder(event));
}
