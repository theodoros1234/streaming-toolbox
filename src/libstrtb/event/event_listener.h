#ifndef STRTB_EVENT_EVENT_LISTENER_H
#define STRTB_EVENT_EVENT_LISTENER_H

#include <cstdint>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <set>
#include <deque>
#include "item.h"
#include "../json/holder.h"

namespace strtb::event {

class system;

typedef struct event_holder {
    uint64_t sub_id = 0;
    json::holder event;
} event_holder;

class event_listener {
private:
    std::set<uint64_t> _subs;
    bool _active = true, _post_first_sub = false;

protected:
    friend system;
    std::mutex _lock;
    std::string _name;
    std::condition_variable _cv;
    std::deque<event_holder> _queue;

public:
    event_listener() = default;
    event_listener(const std::string& name);
    ~event_listener();
    void set_name(const std::string& name);
    uint64_t subscribe(const item_path& event_source, const json::value* param = nullptr);
    void unsubscribe(uint64_t subscription_id);
    void shutdown();
    void reset();
    size_t discard();
    event_holder listen();
    void listen(std::vector<event_holder>& destination);

    /* NOTE: subscribe() doesn't have a resource id variant, as the target event source
     *       may be removed and re-added before or during a subscription (plugin reloads,
     *       reconfiguration, etc.), which would change the resource id. The event system
     *       handles these situations and internally keeps a resource id attached to the
     *       subscription only while the event source is ready.
     */
};

}

#endif // STRTB_EVENT_EVENT_LISTENER_H
