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
        assert(state->rq != nullptr);

        // handle request
        try {
            client::response rs = state->c.send(state->rq);

            // pass the response back to the request object
            std::lock_guard<std::mutex> guard_rq(state->rq->lock);
            state->rq->handler_response = std::move(rs);
            state->rq->cv.notify_one();
        } catch (...) {
            // pass any exception back to the request object
            std::lock_guard<std::mutex> guard_rq(state->rq->lock);
            state->rq->handler_exception = std::current_exception();
            state->rq->cv.notify_one();
        }

        // wait until the full response is received
        state->c.wait_until_idle();

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

            // grab uncancelled pending request from queue
            while (!group->queued_requests.empty()) {
                state->rq = group->queued_requests.front();
                group->queued_requests.pop_front();

                if (state->rq)
                    break;
            }

            if (state->rq) {
                lock.unlock();
                std::lock_guard<std::mutex> guard_rq(state->rq->lock);
                // only use this value as a hint, cause we had to release our lock before setting it
                state->rq->handler_queued = false;
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

bool request_handler::send(client::request::data *rq) {
    std::lock_guard<std::mutex> guard(_lock);

    // find the group that corresponds to this authority (or create it)
    authority_group &group = _groups[rq->https]
                                 .try_emplace(rq->authority, rq->https, rq->authority).first->second;

    // find a waiting thread
    for (auto &t : group.threads) {
        if (!t->rq) {
            t->rq = rq;
            t->cv.notify_one();
            return false;
        }
    }

    // no waiting threads, start a new one
    if (group.threads.size() < STRTB_HTTP_REQUEST_HANDLER_MAX_CONNECTIONS_PER_SERVER) {
        bool created = false;
        try {
            group.threads.push_back(std::unique_ptr<handler_thread>(new handler_thread));
            created = true;
            handler_thread *state = group.threads.back().get();
            state->rq = rq;
            state->thread = std::thread(&request_handler::handler_thread_fn, this, state, &group);
            _thread_count++;
            return false;
        } catch (...) {
            if (created)
                group.threads.pop_back();
            throw;
        }
    }

    // reached max threads, put request into a waiting queue
    group.queued_requests.push_back(rq);
    return true;
}

bool request_handler::cancel(client::request::data *rq) {
    std::lock_guard<std::mutex> guard(_lock);

    // remove this request from the queue
    for (auto &p : _groups[rq->https].at(rq->authority).queued_requests) {
        if (p == rq) {
            p = nullptr;
            return true;
        }
    }

    return false;
}

}