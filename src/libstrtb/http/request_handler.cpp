#include "request_handler.h"
#include "../strescape.h"

#include <chrono>
#include <stdexcept>

namespace strtb::http {

request_handler *request_handler::main = nullptr;

request_handler::authority_group::authority_group(bool https, const std::string &authority)
    : https(https), authority(authority) {}

request_handler::request_handler() : _log("HTTP Client Request Handler", false) {
    if (main)
        throw std::logic_error("only one request handler can exist");
    main = this;
}

request_handler::~request_handler() {
    // stop all threads
    std::unique_lock<std::mutex> lock(_lock);
    _shutdown = true;

    // wake them up
    for (auto &outer_group : _groups)
        for (auto &group : outer_group)
            for (auto &t : group.second.threads)
                t->cv.notify_one();

    // wait until they have all exited
    if (_thread_count > 0)
        _shutdown_cv.wait(lock);

    main = nullptr;
}

void request_handler::handler_thread_fn(handler_thread *state, authority_group *group) {
    while (true) {
        // TODO: handle request
        // TODO: set state in request object

        // wait for something else to happen
        using namespace std::chrono_literals;
        std::unique_lock<std::mutex> lock(_lock);
        // remove the request we just handled
        state->rq = nullptr;

        auto timeout_at = std::chrono::steady_clock::now() + STRTB_HTTP_REQUEST_HANDLER_THREAD_IDLE_TIMEOUT;
        std::cv_status timed_out = std::cv_status::no_timeout;

        while (true) {
            // entire handler being deleted
            if (_shutdown) {
                if (state->rq || !group->queued_requests.empty())
                    _log.warning({"Shutting down while there are still pending requests for ",
                                  string_escape(group->authority)});

                _thread_count--;
                // if this is the last thread, wake up the destructor
                if (_thread_count == 0)
                    _shutdown_cv.notify_one();

                state->thread.detach();
                return;
            }

            // request received
            if (state->rq)
                break;

            // grab pending request from queue
            if (!group->queued_requests.empty()) {
                state->rq = group->queued_requests.front();
                group->queued_requests.pop();
                break;
            }

            // timed out, stop this thread
            if (timed_out == std::cv_status::timeout) {
                _thread_count--;
                state->thread.detach();

                // delete thread state
                for (auto itr = group->threads.begin(); itr < group->threads.end(); itr++) {
                    if (itr->get() == state) {
                        group->threads.erase(itr);

                        // delete entire group if it was emptied
                        if (group->threads.empty())
                            _groups[group->https].erase(group->authority);

                        return;
                    }
                }

                _log.warning({"Failed to delete state on handler thread for ",
                              string_escape(group->authority)});
                return;
            }

            // wait until something happens
            timed_out = state->cv.wait_until(lock, timeout_at);
        }
    }
}

void request_handler::send(client::request::data *rq) {
    std::lock_guard<std::mutex> guard(_lock);

    // find the group that corresponds to this authority (or create it)
    authority_group &group = _groups[rq->https]
                                 .try_emplace(rq->authority, rq->https, rq->authority).first->second;

    // find a waiting thread
    for (auto &t : group.threads) {
        if (!t->rq) {
            t->rq = rq;
            t->cv.notify_one();
            return;
        }
    }

    // no waiting threads, start a new one
    if (group.threads.size() < STRTB_HTTP_REQUEST_HANDLER_MAX_CONNECTIONS_PER_SERVER) {
        bool created = false;
        try {
            group.threads.push_back(std::unique_ptr<handler_thread>(new handler_thread));
            created = true;
            handler_thread *state = group.threads.back().get();
            state->thread = std::thread(&request_handler::handler_thread_fn, this, state, &group);
            _thread_count++;
            return;
        } catch (...) {
            if (created)
                group.threads.pop_back();
            throw;
        }
    }

    // reached max threads, put request into a waiting queue
    group.queued_requests.push(rq);
    // TODO: set appropriate state in request object
}

}