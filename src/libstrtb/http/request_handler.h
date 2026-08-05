#ifndef STRTB_HTTP_REQUEST_HANDLER_H
#define STRTB_HTTP_REQUEST_HANDLER_H

#include "../logging.h"
#include "client.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <map>
#include <memory>
#include <string>

#define STRTB_HTTP_REQUEST_HANDLER_MAX_CONNECTIONS_PER_SERVER 5
#define STRTB_HTTP_REQUEST_HANDLER_THREAD_IDLE_TIMEOUT 60s

namespace strtb::http {

/* FOR INTERNAL USE ONLY
 * Passes HTTP requests to client objects, while automatically handling creation/deletion
 * of client objects, reusing them when appropriate, parallel connections and limiting them, etc.
 */
class request_handler {
private:
    // one handler thread's shared state
    struct handler_thread {
        client c;
        client::request::data *rq = nullptr;
        std::condition_variable cv;
        std::thread thread;
    };

    // group of handler threads and queued requests for a common authority
    struct authority_group {
        std::vector< std::unique_ptr<handler_thread> > threads;
        std::queue<client::request::data*> queued_requests;
        bool https;
        std::string authority;

        authority_group(bool https, const std::string &authority);
    };

    logging::source _log;
    std::mutex _lock;
    bool _shutdown = false;
    std::map<std::string, authority_group> _groups[2];  // [0] = http, [1] = https
    size_t _thread_count = 0;
    std::condition_variable _shutdown_cv;

protected:
    friend client::request;
    friend std::thread;

    void handler_thread_fn(handler_thread *state, authority_group *group);
    void send(client::request::data *rq);
    void cancel(client::request::data *rq);

public:
    static request_handler *main;
    request_handler();
    ~request_handler();
};

}

#endif // STRTB_HTTP_REQUEST_HANDLER_H
