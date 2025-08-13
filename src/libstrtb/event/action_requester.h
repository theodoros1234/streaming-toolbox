#ifndef STRTB_EVENT_ACTION_REQUESTER_H
#define STRTB_EVENT_ACTION_REQUESTER_H

#include "item.h"
#include <memory>
#include <thread>
#include <QObject>

namespace strtb::event {

enum action_request_status {
    ACTION_IDLE,    // no request was sent
    ACTION_PENDING, // request sent, waiting for response
    ACTION_ERROR,   // request sent, error response received
    ACTION_DONE,    // request sent, proper response received
    ACTION_SHUTDOWN // action requester has shut down
};

struct action_request_internal {
    // for use internally by the action requester and handler
    action_request_status status = ACTION_IDLE;
    std::string diagnostic_info;    // for errors, to be displayed to the GUI or to be written to the log
    json::value_object params;
    json::holder returns;
    uint64_t action_sink_id;
    std::mutex lock;
    std::condition_variable cv;
};

struct action_response {
    // to be returned to the plugin
    action_request_status status = ACTION_SHUTDOWN;
    std::string diagnostic_info;
    json::holder returns;
};

class action_requester {
private:
    std::string _name;
    bool _active = true;
    std::shared_ptr<action_request_internal> _rq;
    uint64_t _path_follower_id = 0;

public:
    action_requester();
    action_requester(const std::string& name);
    action_requester(const std::string& name, const item_path& path);
    action_requester(const action_requester&) = delete;
    action_requester(action_requester&) = delete;
    action_requester(const action_requester&&) = delete;
    action_requester(action_requester&&) = delete;
    virtual ~action_requester();
    const std::string& name() const;
    void name_set(const std::string& new_name);
    void path_set(const item_path& path);
    void path_clear();
    void shutdown();
    void restart();
    json::value_object& params() const;
    void run();
    action_response get_response();
};

class action_requester_qt_signal;

class action_requester_qt_signal_emitter : public QObject {
    Q_OBJECT
protected:
    friend action_requester_qt_signal;
    void thread_code(action_requester_qt_signal* parent);

signals:
    void response_received(action_response response);
};

class action_requester_qt_signal : public action_requester {
private:
    std::thread _t;

public:
    action_requester_qt_signal_emitter emitter;

    using action_requester::action_requester;
    virtual ~action_requester_qt_signal();
    void shutdown() = delete;
    void restart() = delete;
    void get_response() = delete;
    void run();
    void cancel();
};

}

#endif // STRTB_EVENT_ACTION_REQUESTER_H
