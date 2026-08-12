#ifndef STRTB_HTTP_CLIENT_IDLE_CONNECTION_HANDLER_CLASS_H
#define STRTB_HTTP_CLIENT_IDLE_CONNECTION_HANDLER_CLASS_H

#include "../logging.h"
#include "../networking/tcp_client.h"
#include "../networking/tcp_client_ssl.h"
#include <variant>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <poll.h>
#include <sys/eventfd.h>

namespace strtb::http {

class client;

/* FOR INTERNAL USE ONLY
 * Handles automatic closing of idle persistent HTTP/1.1 connections. This is defined as a class,
 * so it can be initialized from the main() function after the logging system has been initialized.
 * Function definitions are in client.cpp, so it's easier for client objects to find the instance
 * of this handler.
 */
class client_idle_connection_handler_class {
private:
    struct attach_request_t {
        std::variant<std::monostate, networking::tcp_client, networking::tcp_client_ssl> &socket_container;
        // returned values
        uint64_t &id;   // 0 => error, usage explained in socket info
        size_t &index;  // array index to quickly find the slot during detach
    };

    struct socket_info_t {
        networking::tcp_client* socket = nullptr;
        bool https = false;
        std::chrono::time_point<std::chrono::steady_clock> timeout;
        bool detach_requested = false;
        size_t id = 0;  // used during detach to check if this slot was replaced
    };

    logging::source _log;
    std::mutex _lock;
    std::condition_variable _cv;
    std::thread _t;
    int _eventfd = -1;
    bool _shutdown = false;
    uint64_t _next_id = 1;      // used to generate new IDs, 0 reserved for errors
    std::vector<struct pollfd> _pollfd;         // [0] is eventfd, sockets start from [1]
    std::vector<socket_info_t> _socket_info;    // sockets start from [0], _socket_info[0] -> _pollfd[1]
    std::vector<attach_request_t> _attach_requests;

    void _print_warning();
    // interact with eventfd, return true if there's an error
    bool _event_read();
    bool _event_write();
    bool _attach_apply(socket_info_t &info, pollfd &pfd, size_t index);

protected:
    friend std::thread;
    friend client;
    void thread_function();
    std::pair<uint64_t, size_t> attach(
        std::variant<std::monostate, networking::tcp_client, networking::tcp_client_ssl> &socket_container);
    void detach(uint64_t id, size_t index);

public:
    client_idle_connection_handler_class();
    ~client_idle_connection_handler_class();
};

}

#endif // STRTB_HTTP_CLIENT_IDLE_CONNECTION_HANDLER_CLASS_H
