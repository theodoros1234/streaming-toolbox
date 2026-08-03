#include "shutdown_controller.h"
#include "logging.h"

#include <assert.h>
#include <stdexcept>

namespace strtb {

logging::source log("Shutdown Controller", false);

shutdown_controller::shutdown_controller(shutdown_controller &parent) {
    attach_parent(parent);
}

shutdown_controller::~shutdown_controller() {
    detach_parent();
    if (!_set.empty())
        log.warning({"Destroying while ", _set.size(),
                     " controllable(s) are still attached. This may lead to a crash."});
}

bool shutdown_controller::attach(shutdown_controllable *ctrl) {
    std::lock_guard<std::mutex> guard(_lock);
    assert(ctrl);
    if (!_set.insert(ctrl).second)
        log.warning_one("Attaching controllable that was already attached");
    return _state;
}

void shutdown_controller::detach(shutdown_controllable *ctrl) {
    std::lock_guard<std::mutex> guard(_lock);
    if (_set.erase(ctrl) < 1)
        log.warning_one("Detaching controllable that wasn't attached");
}

void shutdown_controller::_change_state(bool state) {
    if (_state != state) {
        _state = state;

        // send new state to all controllables
        for (auto ctrl : _set)
            ctrl->shutdown_controllable_signal(state);
    }
}

void shutdown_controller::shutdown() {
    std::lock_guard<std::mutex> guard(_lock);
    _change_state(true);
}

void shutdown_controller::reset() {
    std::lock_guard<std::mutex> guard(_lock);

    // ignore reset if parent signalled a shutdown
    if (!_state_parent)
        _change_state(false);
}

void shutdown_controller::attach_parent(shutdown_controller &parent) {
    std::lock_guard<std::mutex> guard(_lock);
    // make sure parent isn't already attached
    if (_parent)
        shutdown_controllable_throw_already_attached();

    // attach and handle new state
    _state_parent = parent.attach(this);
    _parent = &parent;

    // trigger shutdown if parent was already shut down
    if (_state_parent)
        _change_state(true);
}

void shutdown_controller::detach_parent() {
    shutdown_controller *p;

    {
        std::lock_guard<std::mutex> guard(_lock);

        // silently ignore no parent
        if (!_parent)
            return;

        p = _parent;
        _parent = nullptr;
    }

    /* detach outside of mutex-locked area to prevent deadlock
     * if the parent is currently trying to change our state
     */
    p->detach(this);
}

bool shutdown_controller::state() const {
    return _state;
}

void shutdown_controllable::shutdown_controllable_throw_already_attached() const {
    throw std::logic_error("another shutdown controller is already attached");
}

void shutdown_controller::shutdown_controllable_signal(bool state) {
    std::lock_guard<std::mutex> guard(_lock);

    // ignore if parent is currently being detached
    if (!_parent)
        return;

    _state_parent = state;
    _change_state(state);
}

}