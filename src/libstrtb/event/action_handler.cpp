#include "action_handler.h"
#include "action_requester.h"
#include "system.h"
#include "provider.h"
#include "../logging.h"

using namespace strtb::event;

static strtb::logging::source log_s("Event System: Action Handler");

static void _cancel_request(std::shared_ptr<action_request_internal> &r) {
    if (r.get() == nullptr)
        return;

    {
        std::lock_guard<std::mutex> guard(r->lock);
        r->status = ACTION_ERROR;
        r->cv.notify_one();
        r->diagnostic_info = "This action is currently unavailable";
    }
    r.reset();
}

static void _cancel_request(std::shared_ptr<action_request_internal> &r, uint64_t action_sink_id) {
    if (r.get() == nullptr)
        return;
    if (r->action_sink_id != action_sink_id)
        return;

    {
        std::lock_guard<std::mutex> guard(r->lock);
        r->status = ACTION_ERROR;
        r->cv.notify_one();
        r->diagnostic_info = "This action is currently unavailable";
    }
    r.reset();
}

action_handler::request::request(std::shared_ptr<action_request_internal> &&rq, action_handler* parent)
    : _rq(std::move(rq)), _parent(parent) {}

action_handler::request::request(request&& from) : _rq(std::move(from._rq)), _parent(from._parent) {}

action_handler::request::~request() {
    if (_rq.get() == nullptr)
        return;

    std::lock_guard<std::mutex> guard(_rq->lock);
    _rq->status = ACTION_ERROR;
    _rq->cv.notify_one();
    _rq->diagnostic_info = "The action handler couldn't process this action";
}

action_handler::request& action_handler::request::operator=(request&& from) {
    if (_rq.get())
        throw bad_state("the previous request must be answered before move assigning");

    _rq.swap(from._rq);
    _parent = from._parent;
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

    // Type check and make sure the action is still being handled
    std::lock_guard<std::mutex> guard_parent(_parent->_lock);
    auto itr = _parent->_action_sinks.find(_rq->action_sink_id);
    if (itr == _parent->_action_sinks.end()) {
        _cancel_request(_rq);
        _parent = nullptr;
        return;
    }

    try {
        if (_rq->returns.empty())
            throw wrong_type("value not set, consider setting to strtb::json::VAL_NULL "
                             "if it's supposed to return null");
        param_type_check(_rq->returns.value(), itr->second);
    } catch (wrong_type& e) {
        _cancel_request(_rq);
        _parent = nullptr;
        throw wrong_type(std::string("return value failed type check: ") + e.what());
    }

    {
        std::lock_guard<std::mutex> guard(_rq->lock);
        _rq->status = ACTION_DONE;
        _rq->cv.notify_one();
    }
    _rq.reset();
    _parent = nullptr;
}

void action_handler::request::return_error(const char* diagnostic_info) {
    _check();
    {
        std::lock_guard<std::mutex>guard(_rq->lock);
        _rq->status = ACTION_ERROR;
        _rq->cv.notify_one();
        _rq->diagnostic_info = diagnostic_info;
    }
    _rq.reset();
    _parent = nullptr;
}

void action_handler::request::return_error(const std::string& diagnostic_info) {
    _check();
    {
        std::lock_guard<std::mutex>guard(_rq->lock);
        _rq->status = ACTION_ERROR;
        _rq->cv.notify_one();
        _rq->diagnostic_info = diagnostic_info;
    }
    _rq.reset();
    _parent = nullptr;
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
        shutdown();
    } else {
        clear();
    }
}

void action_handler::add(uint64_t target) {
    std::lock_guard<std::mutex> guard_system(system_ptr->_lock);
    std::lock_guard<std::mutex> guard(_lock);
    auto [itr, inserted] = _action_sinks.insert(std::make_pair(target, nullptr));
    if (!inserted)
        throw already_exists("already handling this action sink");

    try {
        itr->second = system_ptr->action_handler_add(_pr.id(), target, this);
    } catch (...) {
        _action_sinks.erase(itr);
    }
}

void action_handler::remove(uint64_t target) {
    std::lock_guard<std::mutex> guard_system(system_ptr->_lock);
    std::lock_guard<std::mutex> guard(_lock);
    if (_action_sinks.erase(target) < 1)
        throw not_found("not handling this action sink, or it doesn't exist");

    for (auto& i : _queue)
        _cancel_request(i, target);
    system_ptr->action_handler_remove(_pr.id(), target, this);
}

void action_handler::clear() {
    std::lock_guard<std::mutex> guard_system(system_ptr->_lock);
    std::lock_guard<std::mutex> guard(_lock);
    system_ptr->action_handler_clear(_pr.id(), _action_sinks, this);
    _action_sinks.clear();
    for (auto& i : _queue)
        _cancel_request(i);
    _queue.clear();
}

void action_handler::start() {
    std::lock_guard<std::mutex> guard(_lock);
    _active = true;
}

void action_handler::shutdown() {
    std::lock_guard<std::mutex> guard_system(system_ptr->_lock);
    std::lock_guard<std::mutex> guard(_lock);
    _active = false;
    _cv.notify_all();
    system_ptr->action_handler_clear(_pr.id(), _action_sinks, this);
    _action_sinks.clear();
    for (auto& r : _queue)
        _cancel_request(r);
    _queue.clear();
}

action_handler::request action_handler::listen() {
    std::unique_lock<std::mutex> guard(_lock);

    while (true) {
        while (_active && _queue.empty())
            _cv.wait(guard);

        if (!_active)
            return request();

        // skip requests for removed action sinks
        if (_queue.front().get() == nullptr || _queue.front()->abandoned) {
            _queue.pop_front();
            continue;
        }

        request new_rq(std::move(_queue.front()), this);
        _queue.pop_front();
        return new_rq;
    }
}

void action_handler::listen(std::vector<request>& destination) {
    std::unique_lock<std::mutex> guard(_lock);
    destination.clear();

    while (true) {
        while (_active && _queue.empty())
            _cv.wait(guard);

        if (!_active)
            return;

        try {
            for (auto& i : _queue)
                if (i.get() != nullptr && !i->abandoned)
                    destination.emplace_back(std::move(i), this);
        } catch (...) {
            destination.clear();
            throw;
        }

        if (!destination.empty())   // only return if we found unabandoned requests
            return;
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
    std::lock_guard<std::mutex> guard(_lock);
    if (_action_sinks.erase(rid) < 1)
        throw internal_error("removing an action sink from an action handler that isn't handling it", log_s, __FILE__, __LINE__, __func__);

    // Cancel all pending requests for this action
    for (auto& i : _queue)
        _cancel_request(i, rid);
}
