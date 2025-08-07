#ifndef STRTB_EVENT_ACTION_HANDLER_H
#define STRTB_EVENT_ACTION_HANDLER_H

#include <memory>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>
#include <set>
#include "item.h"
#include "../json/value.h"

namespace strtb::event {

class provider;
class system;

class action_handler {
private:
    std::mutex _lock;
    std::condition_variable _cv;
    std::deque< std::shared_ptr<action_request> > _queue;
    std::set<uint64_t> _action_sinks;
    provider const& _pr;
    bool _active = false;

protected:
    friend system;
    bool push_request(const std::shared_ptr<action_request>& rq);
    void action_sink_removed(uint64_t rid);

public:
    class request {
    private:
        std::shared_ptr<action_request> _rq;
        void _check() const;

    protected:
        friend action_handler;
        request() = default;    // for empty returns when handler is stopped
        request(std::shared_ptr<action_request>&& rq);

    public:
        request(request&) = delete;
        request(const request&) = delete;
        request(request&& from);
        ~request();
        void return_success(const json::value* value);
        void return_error(const char* diagnostic_info);
        void return_error(const std::string& diagnostic_info);
        uint64_t action_sink_id() const;
        bool empty() const;
    };

    action_handler(provider& provider);
    ~action_handler();
    void add(uint64_t target);     // TODO: make bulk versions of this
    void remove(uint64_t target);
    void clear();
    void start();
    void stop();
    request listen();
    void listen(std::vector<request>& destination);
};

}

#endif // STRTB_EVENT_ACTION_HANDLER_H
