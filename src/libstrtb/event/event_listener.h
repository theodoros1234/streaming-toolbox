#ifndef STRTB_EVENT_EVENT_LISTENER_H
#define STRTB_EVENT_EVENT_LISTENER_H

#include <cstdint>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <set>
#include <deque>
#include <QObject>
#include "item.h"
#include "../json/holder.h"

namespace strtb::event {

class system;

typedef struct event_holder {
    uint64_t sub_id = 0;
    json::holder event;
} event_holder;

class event_listener_base {
protected:
    friend system;
    std::string _name;
    std::set<uint64_t> _subs;
    bool _post_first_sub = false;
    uint64_t _system_subscribe(const item_path& event_source, const json::value* param);
    void _system_unsubscribe(uint64_t subscription_id);
    void _system_unsubscribe(const std::set<uint64_t>& subscription_ids);

    virtual void push_event(uint64_t sub_id, const json::value* event) = 0;
    virtual uint64_t _subscribe(const item_path& event_source, const json::value* param) = 0;

public:
    event_listener_base() = default;
    event_listener_base(const std::string& name);
    virtual ~event_listener_base() = default;
    void set_name(const std::string& name);
    uint64_t subscribe(const item_path& event_source);
    uint64_t subscribe(const item_path& event_source, bool param);
    uint64_t subscribe(const item_path& event_source, int param);
    uint64_t subscribe(const item_path& event_source, long param);
    uint64_t subscribe(const item_path& event_source, long long param);
    uint64_t subscribe(const item_path& event_source, unsigned int param);
    uint64_t subscribe(const item_path& event_source, unsigned long param);
    uint64_t subscribe(const item_path& event_source, unsigned long long param);
    uint64_t subscribe(const item_path& event_source, const char* param);
    uint64_t subscribe(const item_path& event_source, const std::string& param);
    uint64_t subscribe(const item_path& event_source, const json::value* param);

    /* NOTE: subscribe() doesn't have a resource id variant, as the target event source
     *       may be removed and re-added before or during a subscription (plugin reloads,
     *       reconfiguration, etc.), which would change the resource id. The event system
     *       handles these situations and internally keeps a resource id attached to the
     *       subscription only while the event source is ready.
     */
};

class event_listener_queued : public event_listener_base {
private:
    std::mutex _lock;
    bool _active = false;
    std::condition_variable _cv;
    std::deque<event_holder> _queue;

protected:
    virtual void push_event(uint64_t sub_id, const json::value* event);
    virtual uint64_t _subscribe(const item_path& event_source, const json::value* param);

public:
    event_listener_queued() = default;
    event_listener_queued(const std::string& name);
    virtual ~event_listener_queued();
    void unsubscribe(uint64_t subscription_id);
    void stop();
    void start();
    event_holder listen();
    void listen(std::vector<event_holder>& destination);

    /* NOTE: subscribe() doesn't have a resource id variant, as the target event source
     *       may be removed and re-added before or during a subscription (plugin reloads,
     *       reconfiguration, etc.), which would change the resource id. The event system
     *       handles these situations and internally keeps a resource id attached to the
     *       subscription only while the event source is ready.
     */
};

class event_listener_qt_signal;

class event_listener_qt_signal_emitter : public QObject {
    Q_OBJECT
protected:
    friend event_listener_qt_signal;
    void push_event(uint64_t sub_id, const json::value* event);

signals:
    void event_received(uint64_t sub_id, json::holder event);
};

class event_listener_qt_signal : public event_listener_base {
protected:
    virtual void push_event(uint64_t sub_id, const json::value* event);
    virtual uint64_t _subscribe(const item_path& event_source, const json::value* param);

public:
    event_listener_qt_signal_emitter emitter;
    event_listener_qt_signal() = default;
    event_listener_qt_signal(const std::string& name);
    virtual ~event_listener_qt_signal();
    void unsubscribe(uint64_t subscription_id);
    void stop();
};

}

#endif // STRTB_EVENT_EVENT_LISTENER_H
