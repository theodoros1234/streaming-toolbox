#include "action_handler.h"
#include "system.h"
#include "provider.h"

using namespace strtb::event;

action_handler::request::request(std::shared_ptr<action_request>&& rq) : _rq(rq) {}

action_handler::request::request(request&& from) : _rq(std::move(from._rq)) {}

action_handler::request::~request() {
    if (_rq.get() == nullptr)
        return;

    std::lock_guard<std::mutex> guard(_rq->lock);
    _rq->status = ACTION_ERROR;
    _rq->diagnostic_info = "Action handler couldn't process this action";
    _rq->cv.notify_one();
}

void action_handler::request::_check() const {
    if (_rq.get() == nullptr)
        throw bad_state("request was already handled, or was moved to another request handler");
}

void action_handler::request::return_success(const json::value* value) {
    _check();
    std::lock_guard<std::mutex>guard(_rq->lock);
    try {
        _rq->status = ACTION_DONE;
        _rq->returns = value;
        _rq->cv.notify_one();
    } catch (...) {
        _rq->status = ACTION_ERROR;
        _rq->cv.notify_one();
        _rq->diagnostic_info = "Error while returning data from the action. "
                               "The action may still have been processed silently.";
        throw;
    }
}

void action_handler::request::return_error(const char* diagnostic_info) {
    _check();
    std::lock_guard<std::mutex>guard(_rq->lock);
    _rq->status = ACTION_ERROR;
    _rq->cv.notify_one();
    _rq->diagnostic_info = diagnostic_info;
}

void action_handler::request::return_error(const std::string& diagnostic_info) {
    _check();
    std::lock_guard<std::mutex>guard(_rq->lock);
    _rq->status = ACTION_ERROR;
    _rq->cv.notify_one();
    _rq->diagnostic_info = diagnostic_info;
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
    // TODO: Fill this in later. Should cancel any pending requests
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
    _cv.notify_one();
    for (auto& r : _queue) {
        r->status = ACTION_ERROR;
        r->cv.notify_one();
        r->diagnostic_info = "Action is currently unavailable";
        r.reset();
    }
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
