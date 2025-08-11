#include "action_handler.h"
#include "action_requester.h"
#include "system.h"
#include "provider.h"
#include "../logging/logging.h"

using namespace strtb::event;

static strtb::logging::source log_s("Event System: Action Handler");

action_handler::request::request(const std::shared_ptr<action_request_internal>& rq) : _rq(rq) {}

action_handler::request::request(request&& from) : _rq(std::move(from._rq)) {}

action_handler::request::~request() {
    if (_rq.get() == nullptr)
        return;

    std::lock_guard<std::mutex> guard(_rq->lock);
    _rq->status = ACTION_ERROR;
    _rq->diagnostic_info = "Action handler couldn't process this action";
    _rq->cv.notify_one();
}

action_handler::request& action_handler::request::operator=(request&& from) {
    if (_rq.get())
        throw bad_state("the previous request must be answered before move assigning");

    _rq.swap(from._rq);
    return *this;
}

void action_handler::request::_check() const {
    if (_rq.get() == nullptr)
        throw bad_state("request was already handled, or was moved to another request handler");
}

const strtb::json::value_object& action_handler::request::params() const {
    _check();
    // locking not needed, as the requester is not allowed to change this after sending the request
    return _rq->params;
}

strtb::json::holder& action_handler::request::returns() const {
    _check();
    // locking not needed, as the requester will only access this after cv is notified
    return _rq->returns;
}

void action_handler::request::return_success() {
    _check();
    std::lock_guard<std::mutex>guard(_rq->lock);
    _rq->status = ACTION_DONE;
    _rq->cv.notify_one();
    _rq.reset();
}

void action_handler::request::return_error(const char* diagnostic_info) {
    _check();
    std::lock_guard<std::mutex>guard(_rq->lock);
    _rq->status = ACTION_ERROR;
    _rq->cv.notify_one();
    _rq->diagnostic_info = diagnostic_info;
    _rq.reset();
}

void action_handler::request::return_error(const std::string& diagnostic_info) {
    _check();
    std::lock_guard<std::mutex>guard(_rq->lock);
    _rq->status = ACTION_ERROR;
    _rq->cv.notify_one();
    _rq->diagnostic_info = diagnostic_info;
    _rq.reset();
}

uint64_t action_handler::request::action_sink_id() const {
    _check();
    return _rq->action_sink_id;
}

bool action_handler::request::empty() const {
    return _rq.get() == nullptr;
}

action_handler::action_handler(provider& provider) : _pr(provider) {}

action_handler::~action_handler() {
    if (_active) {
        log_s.warning({"Destroying a handler while still active. Stopping, but this could cause a crash."});
        stop();
    }
    clear();
}

// NOTE: add/remove functions should only be called from the same thread as provider item add/remove functions

void action_handler::add(uint64_t target) {
    auto itr = _action_sinks.insert(target);
    if (!itr.second)
        throw already_exists("already handling this action sink");

    try {
        system_ptr->action_handler_add(_pr.id(), target, this);
    } catch (...) {
        _action_sinks.erase(itr.first);
    }
}

void action_handler::remove(uint64_t target) {
    if (_action_sinks.erase(target) < 1)
        throw not_found("not handling this action sink, or it doesn't exist");

    system_ptr->action_handler_remove(_pr.id(), target, this);
}

void action_handler::clear() {
    system_ptr->action_handler_clear(_pr.id(), _action_sinks, this);
    _action_sinks.clear();
}

void action_handler::start() {
    _active = true;
}

void action_handler::stop() {
    std::lock_guard<std::mutex> guard(_lock);
    _active = false;
    _cv.notify_all();
    for (auto& r : _queue) {
        r->status = ACTION_ERROR;
        r->cv.notify_one();
        r->diagnostic_info = "Action is currently unavailable";
        r.reset();
    }
    _queue.clear();
}

action_handler::request action_handler::listen() {
    std::unique_lock<std::mutex> guard(_lock);

    while (_active && _queue.empty())
        _cv.wait(guard);

    if (!_active)
        return request();

    request new_rq(std::move(_queue.front()));
    _queue.pop_front();
    return new_rq;
}

void action_handler::listen(std::vector<request>& destination) {
    std::unique_lock<std::mutex> guard(_lock);
    destination.clear();

    while (_active && _queue.empty())
        _cv.wait(guard);

    if (!_active)
        return;

    try {
        for (auto& i : _queue)
            destination.push_back(std::move(i));
    } catch (...) {
        destination.clear();
        throw;
    }
}

bool action_handler::push_request(const std::shared_ptr<action_request_internal>& rq) {
    std::lock_guard<std::mutex> guard(_lock);
    if (!_active)
        return false;
    _queue.emplace_back(rq);
    _cv.notify_one();
    return true;
}

void action_handler::action_sink_removed(uint64_t rid) {
    if (_action_sinks.erase(rid) < 1)
        throw internal_error("removing an action sink from an action handler that isn't handling it");
}
