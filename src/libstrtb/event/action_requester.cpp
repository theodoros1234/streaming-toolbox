#include "action_requester.h"
#include "system.h"

using namespace strtb::event;

action_requester::action_requester() : _rq(new action_request_internal) {}

action_requester::action_requester(const std::string& name)
    : _name(name), _rq(new action_request_internal) {}

action_requester::action_requester(const std::string& name, const item_path& path)
    : _name(name), _rq(new action_request_internal) {
    path_set(path);
}

action_requester::~action_requester() {
    path_clear();
}

const std::string& action_requester::name() const {
    return _name;
}

void action_requester::name_set(const std::string& new_name) {
    if (_path_follower_id != 0)
        throw bad_state("cannot set a name while the action requester has a path set");
    _name = new_name;
}

void action_requester::path_set(const item_path& path) {
    _path_follower_id = system_ptr->action_requester_path_set(_path_follower_id, path, _name);
}

void action_requester::path_clear() {
    if (_path_follower_id == 0)
        return;
    system_ptr->action_requester_path_clear(_path_follower_id);
    _path_follower_id = 0;
}

void action_requester::shutdown() {
    std::lock_guard<std::mutex> guard(_rq->lock);
    _active = false;
    _rq->cv.notify_one();
}

void action_requester::restart() {
    // replacing _rq prevents a race condition if the handler is still handling the previous request
    _rq.reset(new action_request_internal());
    _active = true;
}

strtb::json::value_object& action_requester::params() const {
    if (_rq->status != ACTION_IDLE)
        throw bad_state("another action is already in progress, get_result() must be called first");
    return _rq->params;
}

void action_requester::run() {
    std::lock_guard<std::mutex> guard(_rq->lock);
    if (_path_follower_id == 0)
        throw bad_state("path not set");
    if (!_active)
        return;
    if (_rq->status == ACTION_SHUTDOWN) // ignored, to be handled by get_result()
        return;
    if (_rq->status != ACTION_IDLE)
        throw bad_state("another action is already in progress, get_result() must be called first");

    system_ptr->action_requester_run(_path_follower_id, _rq);
}

action_response action_requester::get_result() {
    std::unique_lock<std::mutex> guard(_rq->lock);
    if (!_active)
        return action_response();
    if (_rq->status == ACTION_IDLE)
        throw bad_state("no action is running");

    while (_rq->status == ACTION_PENDING && _active)
        _rq->cv.wait(guard);

    if (!_active)
        return action_response();

    action_response returns = {_rq->status, std::move(_rq->diagnostic_info), std::move(_rq->returns)};
    _rq->status = ACTION_IDLE;
    _rq->diagnostic_info.clear();
    // params not cleared for possible reuse
    _rq->returns.clear();
    _rq->action_sink_id = 0;
    return returns;
}
