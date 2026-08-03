#ifndef STRTB_SHUTDOWN_CONTROLLER_H
#define STRTB_SHUTDOWN_CONTROLLER_H

#include <mutex>
#include <set>

namespace strtb {

class shutdown_controller;

class shutdown_controllable {
protected:
    friend shutdown_controller;
    virtual ~shutdown_controllable() = default;
    virtual void shutdown_controllable_signal(bool state) = 0;
    bool shutdown_controllable_attach(shutdown_controller &ctrl);
    void shutdown_controllable_detach(shutdown_controller *ctrl);
    void shutdown_controllable_throw_already_attached() const;
};

/* NOTES:
 * - Make sure all controllables either get destroyed before the controller, or manually detach them.
 */

class shutdown_controller : public shutdown_controllable {
private:
    std::mutex _lock;
    std::set<shutdown_controllable*> _set;
    shutdown_controller *_parent = nullptr;
    bool _state = false, _state_parent = false;     // true = shutdown
    void _change_state(bool state);

protected:
    friend shutdown_controllable;
    bool attach(shutdown_controllable *ctrl);
    void detach(shutdown_controllable *ctrl);
    void shutdown_controllable_signal(bool state);

public:
    shutdown_controller() = default;
    shutdown_controller(shutdown_controller &parent);
    ~shutdown_controller();
    void shutdown();
    void reset();
    void attach_parent(shutdown_controller &parent);
    void detach_parent();
    bool state() const;
};

}

#endif // STRTB_SHUTDOWN_CONTROLLER_H
