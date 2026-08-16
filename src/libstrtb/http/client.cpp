#include "client.h"
#include "request_handler.h"
#include "strescape.h"
#include "../logging.h"
#include "client_idle_connection_handler_class.h"
#include "../base64.h"

#include <assert.h>
#include <charconv>
#include <set>
#include <cstring>
#include <vector>

#define CRLF "\r\n"

using namespace std::string_literals;

namespace strtb::http {

static logging::source log("HTTP Client", false);

static inline unsigned int default_port(bool https) {
    return https ? 443 : 80;
}

static client_idle_connection_handler_class *idle_connection_handler = nullptr;

client_idle_connection_handler_class::client_idle_connection_handler_class() :
    _log("HTTP Client: Idle Connection Handler", false) {
    // only one instance of this can exist
    if (idle_connection_handler)
        throw std::logic_error("only one instance of the HTTP client idle connection handler can exist");

    // create eventfd object
    _eventfd = eventfd(0, EFD_NONBLOCK);
    if (_eventfd == -1) {
        int err = errno;
        _log.error({"Failed to create eventfd object: [errno ", err, "] ", std::strerror(err)});
        _print_warning();
        return;
    }

    // set up polling for eventfd
    _pollfd.push_back({
        .fd = _eventfd,
        .events = POLLIN,
        .revents = 0
    });

    // start handler thread
    try {
        _t = std::thread(&client_idle_connection_handler_class::thread_function, this);
    } catch (std::exception &e) {
        _log.error({"Failed to create background thread: ", e.what()});
        close(_eventfd);
        _eventfd = -1;
        _print_warning();
        _pollfd.clear();
    } catch (...) {
        _log.error_one("Failed to create background thread");
        close(_eventfd);
        _eventfd = -1;
        _print_warning();
        _pollfd.clear();
    }

    idle_connection_handler = this;
}

client_idle_connection_handler_class::~client_idle_connection_handler_class() {
    // stop background thread
    if (_t.joinable()) {
        {
            std::lock_guard<std::mutex> guard(_lock);
            _shutdown = true;

            if (_eventfd != -1)
                _event_write();
        }
        _t.join();
    }

    if (_eventfd != -1)
        close(_eventfd);

    idle_connection_handler = nullptr;
}

void client_idle_connection_handler_class::_print_warning() {
    _log.warning_one("Background cleanup thread failed; this may lead to "
                     "higher resource usage and more frequent failed requests.");
}

bool client_idle_connection_handler_class::_event_read() {
    assert(_eventfd != -1);
    uint64_t value = 0;

    if (read(_eventfd, &value, sizeof(value)) < 0) {
        int err = errno;
        _log.error({"Failed to receive signal over eventfd: [errno ", err, "] ", std::strerror(err)});
        return true;
    }

    return false;
}

bool client_idle_connection_handler_class::_event_write() {
    assert(_eventfd != -1);
    const uint64_t value = 1;

    if (write(_eventfd, &value, sizeof(value)) < 0) {
        int err = errno;

        switch (err) {
        case EAGAIN:    // eventfd counter already at max value, can be ignored
            return false;

        default:
            _log.error({"Failed to send signal over eventfd: [errno ", err, "] ", std::strerror(err)});
            return true;
        }
    }

    return false;
}

bool client_idle_connection_handler_class::_attach_apply(socket_info_t &info, struct pollfd &pfd, size_t index) {
    assert(!_attach_requests.empty());
    assert(info.socket == nullptr);
    if (_next_id == 0) {
        _log.error_one("Out of usable IDs");
        _attach_requests.clear();
        return true;
    }

    auto &rq = _attach_requests.back();
    pfd.events = POLLIN;
    pfd.revents = 0;

#ifndef NDEBUG
    // make sure we're not attaching the same socket multiple times (only for debug mode)
    int _assert_fd = -1;
    if (rq.socket_container.index() == 1)
        _assert_fd = std::get<1>(rq.socket_container).fd();
    else if (rq.socket_container.index() == 2)
        _assert_fd = std::get<2>(rq.socket_container).fd();

    if (_assert_fd != -1)
        for (const auto &p : _pollfd)
            assert(p.fd != _assert_fd);
#endif

    try {
        using namespace std::literals::chrono_literals;
        // get socket based on its type (regular or SSL)
        switch (rq.socket_container.index()) {
        case 1:
            info.https = false;
            info.socket = &std::get<1>(rq.socket_container);
            break;

        case 2:
            info.https = true;
            info.socket = &std::get<2>(rq.socket_container);
            break;

        default:
            pfd.fd = -1;
            _log.error({"Cannot monitor a socket of type ", rq.socket_container.index()});
        }

        if (info.socket) {
            info.id = _next_id;
            info.timeout = std::chrono::steady_clock::now() + 30000ms;  // TODO: make this configurable or based on keep-alive
            pfd.fd = info.socket->fd();
            rq.id = _next_id++;
            rq.index = index;
        }
    } catch (std::exception &e) {
        rq.id = 0;
        rq.index = 0;
        pfd.fd = -1;
        info.id = 0;
        info.socket = nullptr;
        _log.error({"Failed to add socket to idle pool: ", e.what()});
        _attach_requests.pop_back();
        return true;
    }

    _attach_requests.pop_back();
    return false;
}

void client_idle_connection_handler_class::thread_function() {
    _log.info_one("Background thread started");
    bool error = false;

    bool next_timeout_set = false;
    std::chrono::time_point<std::chrono::steady_clock> next_timeout;
    int poll_timeout = -1;

    while (true) {
        assert(_eventfd != -1);
        assert(!_pollfd.empty());

        // determine when the next timeout is
        if (next_timeout_set) {
            // make sure it's still in the future
            auto now = std::chrono::steady_clock::now();
            if (next_timeout > now)
                poll_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(next_timeout - now).count();
            else
                poll_timeout = 0;
        } else {
            poll_timeout = -1;
        }

        // wait for something to happen
        if (poll(_pollfd.data(), _pollfd.size(), poll_timeout) < 0) {
            int err = errno;
            _log.error({"poll() in background thread returned error: [errno ", err, "] ", std::strerror(err)});
            break;
        }

        next_timeout_set = false;

        std::lock_guard<std::mutex> guard(_lock);

        // read event (if any)
        if (_pollfd[0].revents & POLLIN)
            if (_event_read())
                break;  // error (printed to log by _event_read())

        if (_shutdown) {
            // cleanup and stop thread
            close(_eventfd);
            _eventfd = -1;
            _cv.notify_all();
            _log.info_one("Background thread stopped normally");
            return;
        }

        // go through all watched sockets
        for (size_t i = 0; i < _socket_info.size(); i++) {
            using namespace std::literals::chrono_literals;

            auto &info = _socket_info[i];
            auto &pfd = _pollfd[i+1];

            assert((info.socket == nullptr) == (pfd.fd == -1));
            if (info.socket) {
                // check if socket needs closing
                bool needs_close =
                    // socket closed or error
                    (pfd.revents & (POLLIN | POLLHUP | POLLERR)) ||
                    // timed out (or is about to) from our side
                    (std::chrono::steady_clock::now() + 1s > info.timeout);

                if (needs_close) {
                    // TODO: attempt a graceful shutdown for SSL/TLS sockets
                    try {
                        info.socket->close();
                    } catch (std::exception &e) {
                        _log.warning({"Exception while closing an idle connection: ", e.what()});
                    } catch (...) {
                        _log.warning_one("Exception while closing an idle connection");
                    }
                }

                // detach if requested or socket closed
                if (needs_close || info.detach_requested) {
                    pfd.fd = -1;
                    info = {};
                }
            }

            assert((info.socket == nullptr) == (pfd.fd == -1));
            // if empty slot, or we just detached, attach any pending sockets to this (newly freed?) slot
            if (!info.socket && !_attach_requests.empty())
                error = _attach_apply(info, pfd, i) || error;

            // if we end up with a socket on this slot, consider it for the next timeout
            if (info.socket) {
                if (next_timeout_set) {
                    if (info.timeout < next_timeout)
                        next_timeout = info.timeout;
                } else {
                    next_timeout_set = true;
                    next_timeout = info.timeout;
                }
            }
        }

        // handle any remaining attach requests
        while (!_attach_requests.empty()) {
            size_t index = _socket_info.size();
            auto &info = _socket_info.emplace_back();
            auto &pfd = _pollfd.emplace_back();
            error = _attach_apply(info, pfd, index) || error;
        }

        if (error) {
            // handle error before releasing the mutex if it happened in here
            _print_warning();
            close(_eventfd);
            _eventfd = -1;
            _cv.notify_all();
            _log.info_one("Background thread stopped prematurely");
            return;
        }

        // notify anyone waiting (if we were notified first)
        if (_pollfd[0].revents & POLLIN)
            _cv.notify_all();
    }

    // error handling: wake up anyone waiting and exit
    _print_warning();
    std::lock_guard<std::mutex> guard(_lock);
    close(_eventfd);
    _eventfd = -1;
    _cv.notify_all();
    _log.info_one("Background thread stopped prematurely");
}

std::pair<uint64_t, size_t> client_idle_connection_handler_class::attach(
    std::variant<std::monostate, networking::tcp_client, networking::tcp_client_ssl> &socket_container) {
    std::unique_lock<std::mutex> lock(_lock);

    // make sure the background thread hasn't failed
    if (_eventfd == -1)
        return {0, 0};

    try {
        // request to be attached and wait
        uint64_t id = 0;    // if it stays at 0 => error
        size_t index = 0;

        _attach_requests.push_back({socket_container, id, index});
        if (_event_write()) {
            _log.error_one("Failed to notify background thread during attach");
            return {0, 0};
        }
        _cv.wait(lock);

        return {id, index};
    } catch (std::exception &e) {
        _log.error({"Failed to attach socket: ", e.what()});
    }

    return {0, 0};
}

void client_idle_connection_handler_class::detach(uint64_t id, size_t index) {
    std::unique_lock<std::mutex> lock(_lock);

    // make sure the background thread hasn't failed
    if (_eventfd == -1)
        return;

    assert(id);
    auto& info = _socket_info.at(index);

    // was our socket already closed?
    if (info.id != id)
        return;

    // wait until it's detached
    info.detach_requested = true;
    if (_event_write())
        _log.error_one("Failed to notify background thread during detach");
    _cv.wait(lock);
}

client::client() {}

client::~client() {
    detach_shutdown_controller();

    if (_request) {
        log.warning_one("Destroying while a request object is still connected. "
                        "Attempting to disconnect, but this may cause a crash.");
        shutdown();
        _request->c = nullptr;
    }

    if (_response) {
        log.warning_one("Destroying while a response object is still connected. "
                        "Attempting to disconnect, but this may cause a crash.");
        shutdown();
        _response->c = nullptr;
    }

    _cancel();
}

void client::_shutdown_check_early() {
    // try to detect a shutdown early (before the request is sent)
    if (_is_shutdown)
        throw in_shutdown_state("http client was shut down");
    if (_is_shutdown_rq)
        throw in_shutdown_state("http request was shut down");
    if (_is_shutdown_rs)
        throw in_shutdown_state("http response was shut down");
}

void client::_shutdown_check() {
    std::lock_guard<std::mutex> guard(_lock);
    _shutdown_check_early();
}

void client::_shutdown() {
    _is_shutdown = true;
    if (_socket)
        _socket->cancel_connect();
}

void client::shutdown() {
    std::lock_guard<std::mutex> guard(_lock);
    _shutdown();
}

void client::shutdown_request() {
    std::lock_guard<std::mutex> guard(_lock);
    assert(_request);
    _is_shutdown_rq = true;
    if (_socket)
        _socket->cancel_connect();
}

void client::shutdown_response() {
    std::lock_guard<std::mutex> guard(_lock);
    assert(_response);
    _is_shutdown_rs = true;
    if (_socket)
        _socket->cancel_connect();
}

void client::shutdown_controllable_signal(bool state) {
    std::lock_guard<std::mutex> guard(_lock);
    // ignore if in the middle of detaching
    if (!_shutdown_controller)
        return;

    _shutdown_controller_state = state;
    if (state)
        _shutdown();
    else
        _reset();
}

void client::_reset() {
    _is_shutdown = false;
    if (_socket)
        _socket->reset();
}

void client::reset() {
    std::lock_guard<std::mutex> guard(_lock);
    // only reset if controller isn't shut down
    if (!_shutdown_controller_state)
        _reset();
}

void client::attach_shutdown_controller(shutdown_controller &ctrl) {
    std::lock_guard<std::mutex> guard(_lock);
    if (_shutdown_controller)
        shutdown_controllable_throw_already_attached();

    _shutdown_controller_state = shutdown_controllable_attach(ctrl);
    _shutdown_controller = &ctrl;

    // handle past shutdown
    if (_shutdown_controller_state)
        _shutdown();
}

void client::detach_shutdown_controller() {
    shutdown_controller *p;

    {
        std::lock_guard<std::mutex> guard(_lock);

        // silently ignore no controller
        if (!_shutdown_controller)
            return;

        p = _shutdown_controller;
        _shutdown_controller = nullptr;
        _shutdown_controller_state = false;
    }

    shutdown_controllable_detach(p);
}

void client::_cancel() {
    // first, detach socket from background handler thread
    _idle_handler_detach();

    if (_socket) {
        if (_socket->is_open())
            _socket->close();
    }
    _decoders.clear();
}

void client::_cancel_response() {
    assert(_response);
    _cancel();
    _response = nullptr;
    _cv.notify_one();
}

void client::cancel_request() {
    std::lock_guard<std::mutex> guard(_lock);
    assert(_request);
    _cancel();
    _request = nullptr;
    _cv.notify_one();
}

void client::cancel_response() {
    std::lock_guard<std::mutex> guard(_lock);
    _cancel_response();
}

void client::_finish_response(bool detach) {
    _response = nullptr;
    _cv.notify_one();
    if (!detach) {
        if (_keepalive)     // pass the socket to the background handler thread
            _idle_handler_attach();
        else    // instantly close
            _socket->close();
    }
    _decoders.clear();
}

void client::wait_until_idle() {
    std::unique_lock<std::mutex> lock(_lock);
    while (_request || _response)
        _cv.wait(lock);
}

void client::_idle_handler_attach() {
    if (idle_connection_handler)
        std::tie(_idle_handler_id, _idle_handler_index) =
            idle_connection_handler->attach(_socket_container);
}

void client::_idle_handler_detach() {
    if (_idle_handler_id && idle_connection_handler) {
        idle_connection_handler->detach(_idle_handler_id, _idle_handler_index);
        _idle_handler_id = 0;
        _idle_handler_index = 0;
    }
}

static std::string make_header_line(const std::string &name, const std::string &value) {
    return name + ": " + value + CRLF;
    // TODO: make this send stuff directly after adding std::string_view support to tcp_socket::send();
}

bool client::_connection_reusable(const std::string &authority, bool https, bool autoclose) {
    bool sock_open = _socket && _socket->is_open();     // open connection
    bool auth_matches = _authority == authority &&      // matching authority (host and port)
                        ((https && _socket_container.index() == 2) ||   // matching encryption
                         (!https && _socket_container.index() == 1));

    // close socket if we're about to connect to another server
    if (autoclose && sock_open && !auth_matches)
        _socket->close();

    return sock_open && auth_matches;
}

client::response client::send(request &r) {
    // request must be prepared
    r._ready_to_send();
    return send(r._d);
}

client::response client::send(request::data *r) {
    if (_request || _response)
        throw bad_state("another request is in progress");

    _is_shutdown_rq = false;
    _is_shutdown_rs = false;
    bool retriable = false;

    try {
        // set up the appropriate socket and connect
        bool connection_reusable = false;
        if (r->https) {
            // https
            networking::tcp_client_ssl *s = nullptr;
            {
                std::lock_guard<std::mutex> guard_rq(r->lock);
                std::lock_guard<std::mutex> guard(_lock);

                // check for request shutdown before attaching to the request
                _is_shutdown_rq = r->shutdown_ctrl_state || r->cancelling;
                _shutdown_check_early();
                _request = r;
                _request->c = this;

                // get back our socket (if we had one)
                _idle_handler_detach();

                if (_socket_container.index() == 2) {
                    // reuse existing socket
                    connection_reusable = _connection_reusable(_request->authority, true, true);
                    _socket->reset();
                    s = &std::get<2>(_socket_container);
                } else {
                    // create required socket type
                    if (_socket && _socket->is_open())  // close the old one
                        _socket->close();
                    s = &_socket_container.emplace<2>(true, false);
                    _socket = s;
                    _authority.clear();
                }
            }

            if (!connection_reusable)
                s->connect(_request->host, _request->port, false, !_request->allow_invalid_cert);
        } else {
            // http
            {
                std::lock_guard<std::mutex> guard_rq(r->lock);
                std::lock_guard<std::mutex> guard(_lock);

                // check for request shutdown before attaching to the request
                _is_shutdown_rq = r->shutdown_ctrl_state || r->cancelling;
                _shutdown_check_early();
                _request = r;
                _request->c = this;

                // get back our socket (if we had one)
                _idle_handler_detach();

                if (_socket_container.index() == 1) {
                    // reuse existing socket
                    connection_reusable = _connection_reusable(_request->authority, false, true);
                    _socket->reset();
                } else {
                    // create required socket type
                    if (_socket && _socket->is_open())  // close the old one
                        _socket->close();
                    _socket = &_socket_container.emplace<1>(true);
                    _authority.clear();
                }
            }

            if (!connection_reusable)
                _socket->connect(_request->host, _request->port);
        }

        _authority = _request->authority;

        // connection auto-retriable only if an idempotent request fails on a reused persistent connection
        retriable = connection_reusable && _request->method_idempotent;

        // send request line
        _socket->send(_request->method + " " + _request->path + " HTTP/1.1" CRLF);

        // upgrade options
        // TODO: after implementing HTTP/2, copy this vector and add h2 as an option
        const std::vector<product> &upgrade = _request->upgrade;

        // connection options
        std::vector<std::string> connection;
        if (!upgrade.empty())
            connection.push_back("upgrade");
        // TODO: option to close, and include TE, Upgrade, etc. when necessary
        _keepalive = true;

        // decide if we're sending a body
        std::string content_length;
        if (_request->body_set)
            content_length = std::to_string(_request->body_str.length());   // TODO: locale issues

        // send headers, prioritizing some, with default values, and with restrictions
        struct priority_headers_t {
            std::string name, value;
            bool allow_override,    // allows the requester to override the default value
                 send_default;      // send the default value (or ignore)
        };

        // WARNING: always use lowercase names, and never use values that may contain CRLF
        std::initializer_list<priority_headers_t> priority_headers = {
            {"host"s, _authority, false, true},
            {"user-agent"s, get_default_user_agent(), true, true},
            {"connection"s, list_to_string(connection), false, !connection.empty()},
            {"upgrade"s, list_to_string(upgrade), false, !upgrade.empty()},
            {"accept-encoding"s, get_supported_decoders_str(), false, !get_supported_decoders_str().empty()},
            {"content-length"s, std::move(content_length), false, _request->body_set}
        };

        for (const auto &h : priority_headers) {
            auto h_existing = _request->headers.find(h.name);
            if (h_existing == _request->headers.end()) {
                // send the default value we defined above
                if (h.send_default)
                    _socket->send(make_header_line(h.name, h.value));
            } else {
                // send existing header if allowed, otherwise just send the default
                if (h.allow_override)
                    _socket->send(make_header_line(h_existing->first, h_existing->second));
                else if (h.send_default)
                    _socket->send(make_header_line(h.name, h.value));
                _request->headers.erase(h_existing);
            }
        }

        // send remaining headers
        for (const auto &h : _request->headers)
            _socket->send(make_header_line(h.first, h.second));

        // empty line to mark end of headers
        _socket->send(CRLF);

        // create response object
        response rs(this);
        _response = rs._d;
        _response->recv_mode = _request->recv_mode;
        bool response_handled = false;

        // send body (if set)
        if (_request->body_set) {
            size_t chunk_size = _socket->buffer_size();
            size_t total = _request->body_str.length();
            const char *data = _request->body_str.data();

            for (size_t i = 0; i < total; i += chunk_size) {
                // monitor for error/closure response
                if (!response_handled && _socket->available()) {
                    if (_handle_response(rs, retriable, upgrade)) {
                        // 1xx informational response
                        // TODO: handle 100-continue responses
                        if (_response->status == 101)   // switching protocol after finishing request
                            response_handled = true;
                        else
                            rs.soft_clear();
                    } else {
                        response_handled = true;

                        // abort upload on connecture closure or error response
                        if (!_keepalive || (_response->status / 100) != 2) {
                            std::lock_guard<std::mutex> guard(_lock);

                            // make sure this connection won't be reused
                            _keepalive = false;

                            // if we handled receiving the response body, close immediately
                            if (!_response)
                                _cancel();

                            throw incomplete_upload(std::move(rs));
                        }
                    }
                }

                // don't send more data than is left
                chunk_size = std::min(chunk_size, total - i);
                _socket->send(data + i, chunk_size);
            }
        }
        _socket->flush();
        // TODO: ability to send files or stream the body contents

        while (!response_handled) {
            if (_handle_response(rs, retriable, upgrade) && _response->status != 101) {
                // 1xx informational response (except for valid 101 switching protocol)
                rs.soft_clear();
            } else {
                // regular response or valid 101
                response_handled = true;
            }
        }

        // detach socket when switching protocol
        if (_response->status == 101) {
            networking::tcp_client* s = _socket;
            shutdown_controller *ctrl = nullptr;

            {
                std::lock_guard<std::mutex> guard_rq(_request->lock);
                std::lock_guard<std::mutex> guard(_lock);

                // move socket to response and detach from it
                _response->socket = std::move(_socket_container);
                _socket = nullptr;
                _response->c = nullptr;
                _finish_response(true);

                // detach from request
                ctrl = _request->shutdown_ctrl;
                _request->c = nullptr;
                _request = nullptr;
            }

            // any missed shutdown signal in this gap will be delivered by the attachment below

            // attach request's shutdown controller to socket
            if (ctrl)
                s->attach_shutdown_controller(*ctrl);

        }

        return rs;
    } catch (incomplete_upload&) {
        throw;
    } catch (in_shutdown_state&) {
        if (_request) {
            std::lock_guard<std::mutex> guard_rq(_request->lock);
            _request->c = nullptr;
            cancel_request();
        }
        throw;
    } catch (...) {
        if (_request) {
            std::lock_guard<std::mutex> guard_rq(_request->lock);
            _request->c = nullptr;
            cancel_request();
        }
        // check if the error was caused by a shutdown
        _shutdown_check();

        // auto-retry if it's safe to do so
        if (retriable)
            return send(r);
        else
            throw;
    }
}

bool client::_handle_response(response &rs, bool &retriable, const std::vector<product> &rq_upgrade) {
    // receive response status-line
    // request stops being retriable as soon as any data is received
    std::string line;
    if (!_socket->recv_line(line, true, CRLF, STRTB_HTTP_STATUS_LINE_MAX_LEN)) {
        if (line.length() == STRTB_HTTP_STATUS_LINE_MAX_LEN) {
            retriable = false;
            throw invalid_message("response status line too long");
        } else if (line.empty()) {
            throw incomplete_message("server closed the connection before anything was received");
        } else {
            retriable = false;
            throw invalid_message("incomplete response status line");
        }
    }
    retriable = false;

    auto status_line = parse_status_line(line);
    if (!status_line.valid)
        throw invalid_message("invalid response status line");
    if (status_line.version.major != 1)
        throw invalid_message("incompatible response HTTP version");
    _response->status = status_line.status_code;
    _response->status_message = std::move(status_line.reason_phrase);
    _response->version = status_line.version;

    // receive response headers
    while (true) {
        // TODO: obs-fold is allowed, give a param to enable/disable that
        // TODO: limit max amount of headers and trailers to receive

        if (!_socket->recv_line(line, true, CRLF, STRTB_HTTP_FIELD_LINE_MAX_LEN)) {
            if (line.length() >= STRTB_HTTP_FIELD_LINE_MAX_LEN)
                throw unsupported_message("response header line too long");
            else
                throw invalid_message("incomplete response header line");
        }

        // empty line marks end of headers
        if (line.empty())
            break;

        if (!_response->headers.process_line(line))
            throw invalid_message("invalid response header line");
    }

    // check connection options
    std::set<std::string> rs_connection_options;
    auto rs_connection_options_header = _response->headers.fields.find("connection");
    if (rs_connection_options_header != _response->headers.fields.end()) {
        auto rs_connection_options_list = parse_field_token_list(rs_connection_options_header->second, false);
        if (!rs_connection_options_list.valid)
            throw invalid_message("invalid response connection header");

        for (const auto &opt : rs_connection_options_list.list)
            rs_connection_options.insert(std::move(opt));
    }

    // determine if the connection can be reused
    if (rs_connection_options.count("close") || _response->version.minor < 1)
        _keepalive = false;

    // TODO: Transfer-Encoding in HTTP/1.0 MUST be treated as faulty framing and close the connection afterwards

    // stop here if it's a 1xx informational request
    if (_response->status / 100 == 1) {
        // switching protocols
        if (_response->status == 101) {
            // check if upgrade is valid
            // only HTTP/1.1
            if (_response->version.major != 1 || _response->version.minor != 1)
                throw invalid_message("protocol switching only allowed in HTTP/1.1");

            // connection: upgrade
            auto rs_upgrade_header = _response->headers.fields.find("upgrade");
            if (!rs_connection_options.count("upgrade") || rs_upgrade_header == _response->headers.fields.end())
                throw invalid_message("server switched protocols without sending the required headers");

            // upgrade header
            auto rs_upgrade = parse_field_upgrade(rs_upgrade_header->second);
            if (!rs_upgrade.valid || rs_upgrade.list.empty())
                throw invalid_message("invalid response upgrade header");

            // check if the new protocol was in our choices
            // using a crappy O(N^2) search cause there shouldn't be many upgrade options
            for (const auto &rs_option : rs_upgrade.list) {
                bool found = false;
                for (const auto &rq_option : rq_upgrade) {
                    if (rs_option == rq_option) {
                        found = true;
                        break;
                    }
                }

                if (!found)
                    throw invalid_message("server upgraded to a protocol we didn't ask for");
            }

            _response->upgrade = std::move(rs_upgrade.list);
        }

        return true;
    }

    // determine if a body is present
    if (_request->method == "HEAD" || _response->status == 204 ||
        _response->status == 304) {
        // certain methods and status codes cannot have a body
        // close connection if there's no body
        std::lock_guard<std::mutex> guard(_lock);
        _response->c = nullptr;
        _finish_response();
    } else {
        auto te = _response->headers.fields.find("transfer-encoding");
        if (te != _response->headers.fields.end()) {
            // parse transfer-encoding header
            auto te_parsed = parse_field_token_params_list(te->second, false, true);
            if (!te_parsed.valid)
                throw invalid_message("invalid response transfer encoding");

            if (!transfer_encoding_make_decoders(_decoders, te_parsed.list, _response->trailers, *_socket, true))
                _keepalive = false;
        } else {
            auto ce = _response->headers.fields.find("content-length");

            if (ce != _response->headers.fields.end()) {
                // parse content-length header
                auto ce_parsed = parse_field_integer(ce->second);
                if (!ce_parsed.valid) {
                    if (ce_parsed.overflow)
                        throw unsupported_message("response content length is too long");
                    else
                        throw invalid_message("invalid response content length");
                }

                _decoders.push_back(std::unique_ptr<decoder>(new body_fixed_length(*_socket, ce_parsed.number)));
            } else {
                // no encoding or length info
                _decoders.push_back(std::unique_ptr<decoder>(new body_until_close(*_socket)));
                _keepalive = false;
            }
        }

        // check, parse and handle content-encoding
        auto ce = _response->headers.fields.find("content-encoding");
        if (ce != _response->headers.fields.end()) {
            auto ce_parsed = parse_field_token_list(ce->second, false);
            if (!ce_parsed.valid)
                throw invalid_message("invalid response content encoding");

            content_encoding_make_decoders(_decoders, ce_parsed.list);
        }
    }

    // automatically receive body, if requested
    if (_request->recv_mode == RECV_STR) {
        _response->c = nullptr;
        std::string &body = _response->body_str;
        size_t max_len = _request->recv_max_len;

        // receive until we reach end of body or exceed the length limit (error)
        while (true) {
            std::string_view chunk = recv_body(0);
            // NOTE: recv_body handles detaching on end-of-body or error

            if (chunk.empty())  // end of body
                break;

            if (chunk.length() + body.length() > max_len) { // exceeded length limit
                cancel_response();
                throw unsupported_message("body length exceeds the configured limit for this receive method");
            }

            body += chunk;
        }
    }

    shutdown_controller *ctrl = nullptr;
    // detach from request
    {
        std::lock_guard<std::mutex> guard_rq(_request->lock);
        std::lock_guard<std::mutex> guard(_lock);
        ctrl = _request->shutdown_ctrl;
        _request->c = nullptr;
        _request = nullptr;
    }

    // any missed shutdown signal in this gap will be delivered by the attachment below

    // attach request's shutdown controller to response
    if (ctrl)
        rs.attach_shutdown_controller(*ctrl);

    return false;
}

std::string_view client::recv_body(size_t max_len) {
    assert(_response);

    try {
        assert(!_decoders.empty());
        if (!_decoders.empty()) {
            auto [data, len] = _decoders.back()->read(max_len);

            if (len == 0) {     // reading 0 bytes means we reached the end of the body
                // unless a shutdown truncated part of the body and somehow didn't cause an error
                std::lock_guard<std::mutex> guard_rs(_response->lock);
                std::lock_guard<std::mutex> guard(_lock);
                _shutdown_check_early();
                _response->c = nullptr;
                _finish_response();
                // TODO: attempt graceful shutdown over TLS
            }

            return std::string_view(data, len);
        } else {
            return std::string_view();
        }
    } catch (in_shutdown_state&) {
        std::lock_guard<std::mutex> guard_rs(_response->lock);
        _response->c = nullptr;
        cancel_response();
        throw;
    } catch (...) {
        std::lock_guard<std::mutex> guard_rs(_response->lock);
        std::lock_guard<std::mutex> guard(_lock);
        _response->c = nullptr;
        _cancel_response();
        // check if the error was caused by a shutdown
        _shutdown_check_early();
        throw;
    }
}

const std::string& client::authority() const {
    return _authority;
}

bool client::encrypted() const {
    return _socket_container.index() == 2;
}

client::request::request(std::string_view method) {
    _d = new data;

    // check validity
    auto [method_to, method_valid] = parse_token(method);
    if (!method_valid || method_to != method.length())
        throw std::invalid_argument("invalid request method");
    _d->method = method;

    // check if it's safe and/or idempotent
    // TODO: faster string matching
    _d->method_safe = _d->method == "GET" || _d->method == "HEAD" || _d->method == "OPTIONS" || _d->method == "TRACE";
    _d->method_idempotent = _d->method_safe || _d->method == "PUT" || _d->method == "DELETE";
}

client::request::~request() {
    clear();
}

void client::request::_move(request &&other) {
    // NOTE: clearing this object's data must be done by caller if necessary

    // temporarily detach any shutdown controller
    bool ctrl_attached = false;
    if (other._d && other._d->shutdown_ctrl) {
        ctrl_attached = true;
        other.shutdown_controllable_detach(other._d->shutdown_ctrl);
        // DO NOT clear other._d->shutdown_ctrl cause the client might wanna pass it to a response
    }

    // move over the data struct
    _d = other._d;
    other._d = nullptr;

    // reattach the shutdown controller
    if (ctrl_attached) {
        std::lock_guard<std::mutex> guard(_d->lock);
        _d->shutdown_ctrl_state = shutdown_controllable_attach(*_d->shutdown_ctrl);
        if (_d->shutdown_ctrl_state)
            _shutdown();
    }
}

client::request::request(request &&other) {
    _move(std::move(other));
}

client::request& client::request::operator=(request &&other) {
    clear();
    _move(std::move(other));
    return *this;
}

void client::request::_cancel(std::unique_lock<std::mutex> &lock) {
    // TODO: have a look at thread safety again after implementing the request handling system
    if (_d->handler_used) {
        // request handler used, shut down and wait
        _d->cancelling = true;
        _shutdown();

        while (_d->handler_response.empty() && !_d->handler_exception)
            _d->cv.wait(lock);

        // ignore returned stuff and clean up
        _d->cancelling = false;
        _d->handler_response.clear();
        _d->handler_exception = nullptr;
        _d->handler_used = false;
    } else {
        // http client used, safe to cancel from here
        if (_d->c) {
            _d->c->cancel_request();
            _d->c = nullptr;
        }
    }
}

void client::request::cancel() {
    if (_d) {
        std::unique_lock<std::mutex> lock(_d->lock);
        _cancel(lock);
    }
}

void client::request::clear() {
    if (_d) {
        {
            std::unique_lock<std::mutex> lock(_d->lock);
            if (_d->shutdown_ctrl)
                shutdown_controllable_detach(_d->shutdown_ctrl);
            _cancel(lock);
        }
        delete _d;
        _d = nullptr;
    }
}

void client::request::_shutdown() {
    if (_d->c) {    // being processed by a client
        _d->c->shutdown_request();
    } else if (_d->handler_queued) {    // sitting in a handler queue
        if (request_handler::main->cancel(_d)) {
            // successfully removed from queue, wake up waiting thread
            _d->handler_exception = std::make_exception_ptr(in_shutdown_state("http request was shut down"));
            _d->cv.notify_one();
        }
        // if not removed, a handler thread has just grabbed it and will handle the shutdown
    }
}

void client::request::_valid_state(bool running) {
    // make sure this request is in a valid state (not moved or (not) in-progress)
    if (!_d)
        throw bad_state("request is empty (moved or not set)");

    if ((_d->handler_used || _d->c) != running) {
        if (running)
            throw bad_state("request not in progress");
        else
            throw bad_state("request already in progress");
    }
}

client::request&& client::request::with_url(std::string_view url) {
    _valid_state(false);

    try {
        uri::parser parser;

        // parse URL
        auto [url_to, url_valid] = parser.parse_uri(url, false);
        if (!url_valid || parser.path_type != uri::PATH_ABEMPTY)
            throw std::invalid_argument("invalid or incompatible URL "
                                        "(note that IPv6 addresses must be enclosed in square brackets)");

        // check scheme
        _d->https = false;
        std::string scheme = parser.scheme_str(url, true);
        if (scheme == "https")
            _d->https = true;
        else if (scheme != "http")
            throw std::invalid_argument("incompatible URL scheme, only http/https allowed");

        // check host and port
        std::string host = parser.host_str(url, true);
        int port = -1;
        if (parser.port_from == parser.port_to) // no port specified, use default
            port = default_port(_d->https);
        else    // a port was specified
            port = parser.port_uint16(url);     // -1 error will be handled by _d->with_parsed_host
        _with_parsed_host(_d->https, host, parser.host_type, port);

        // path
        // TODO: handle any double slashes and dot segments
        _d->path_asterisk = false;
        // empty path corresponds to /
        _d->path = (parser.path_from == parser.path_to) ? "/" : parser.path_str(url);

        // query
        _d->query_set = parser.query_to;
        if (_d->query_set) {
            _d->path += '?';
            _d->path.append(url.data() + parser.query_from, parser.query_to - parser.query_from);
        }

        // fragment is ignored
    } catch (...) {
        _d->https = false;
        _d->port = -1;
        _d->path_asterisk = false;
        _d->query_set = false;
        _d->authority.clear();
        _d->host.clear();
        _d->path.clear();
        throw;
    }

    return std::move(*this);
}

// checks the host and port, stores them, and creates authority string (https must be stored by caller)
void client::request::_with_parsed_host(bool https, std::string_view host, uri::host_type_enum type, unsigned int port) {
    // check host
    switch (type) {
    case uri::HOST_REGNAME:
    case uri::HOST_IPV4:
        _d->host = host;
        break;

    case uri::HOST_IPV6:
        _d->host = host.substr(1, host.size() - 2);    // trim square brackets
        break;

    case uri::HOST_EMPTY:
        throw std::invalid_argument("missing host");

    case uri::HOST_IPVFUTURE:
    default:
        throw std::invalid_argument("invalid or unsupported IP address type");
    }

    // check port range
    if (port >= 65536)
        throw std::invalid_argument("invalid port number");
    // check for unsafe ports
    if (!_d->allow_unsafe_ports && _d->port != 80 && port != 443 && is_unsafe_port(port))
        throw security_precaution("blocked access to unsafe port");
    _d->port = port;

    // set authority (used by host header)
    if (port == default_port(https)) {  // no need to specify the default port
        _d->authority = host;
    } else {
        // convert port to str without locale issues
        char port_str[6] = {0};
        std::to_chars(port_str, &port_str[sizeof(port_str) - 1], port);
        _d->authority = host + ":" + port_str;
    }
}

client::request&& client::request::with_host(bool https, std::string_view hostname) {
    return with_host(https, hostname, default_port(https));
}

client::request&& client::request::with_host(bool https, std::string_view hostname, unsigned int port) {
    _valid_state(false);

    try {
        _d->https = https;

        // parse host
        uri::parser parser;
        auto [hostname_to, hostname_valid] = parser.parse_host(hostname, false);
        if (!hostname_valid)
            throw std::invalid_argument("invalid hostname or IP address "
                                        "(note that IPv6 addresses must be enclosed in square brackets)");
        std::string host_str = parser.host_str(hostname, true);

        _with_parsed_host(https, host_str, parser.host_type, port);
    } catch (...) {
        _d->https = false;
        _d->port = -1;
        _d->host.clear();
        _d->authority.clear();
        throw;
    }

    return std::move(*this);
}

// NOTE: only allows origin form and asterisk form, maybe change this later if it's a problem
client::request&& client::request::with_path(std::string_view path) {
    _valid_state(false);

    try {
        _d->query_set = false;
        _d->path_asterisk = (path.size() == 1 && path[0] == '*');

        if (_d->path_asterisk) {   // asterisk-form (used for OPTIONS requests)
            _d->path = path;
        } else {    // origin-form (used by most methods)
            uri::parser parser;

            // handle path
            if (!parser.parse_relative_ref(path, false).second)
                throw std::invalid_argument("invalid or unsupported path");

            switch (parser.path_type) {
            case uri::PATH_ABSOLUTE:
                _d->path = parser.path_str(path);
                break;

            case uri::PATH_EMPTY:   // empty path corresponds to /
                _d->path = "/";
                break;

            default:
                throw std::invalid_argument("invalid or unsupported path");
            }

            // handle query
            if (parser.query_to) {
                _d->query_set = true;
                _d->path += '?';
                _d->path.append(path.data() + parser.query_from, parser.query_to - parser.query_from);
            }

            // ignore fragment
        }
    } catch (...) {
        _d->query_set = false;
        _d->path_asterisk = false;
        _d->path.clear();
        throw;
    }

    return std::move(*this);
}

void client::request::_with_header_trust_name(const std::string &name, std::string_view value) {
    // check value for invalid characters
    for (char c : value)
        if (!(is_vchar(c) || is_obs_text(c) || is_whitespace(c)))
            std::invalid_argument("value contains invalid character " + char_escape(c));

    try {
        _d->headers[name] = value;
    } catch (...) {
        _d->headers.erase(name);
        throw;
    }
}

client::request&& client::request::with_header(std::string_view name, std::string_view value) {
    _valid_state(false);

    // case-insensitive name
    std::string name_tolower = parse_token_tolower(name);
    if (name_tolower.empty() || name_tolower.length() != name.length())
        throw std::invalid_argument("invalid header name");

    _with_header_trust_name(name_tolower, value);

    return std::move(*this);
}

template<class T> client::request&& client::request::_with_headers(T headers) {
    _valid_state(false);
    _d->headers.clear();

    try {
        // replace all headers with the new header list (duplicates will be silently ignored)
        for (const auto& [name, value] : headers)
            with_header(name, value);
    } catch (...) {
        _d->headers.clear();
        throw;
    }

    return std::move(*this);
}

client::request&& client::request::with_headers(const std::map<std::string, std::string> &headers) {
    return _with_headers<const std::map<std::string, std::string> &>(headers);
}

client::request&& client::request::with_headers(const std::vector< std::pair<std::string, std::string> > &headers) {
    return _with_headers<const std::vector< std::pair<std::string, std::string> > &>(headers);
}

client::request&& client::request::with_headers(
        std::initializer_list< std::pair<std::string_view, std::string_view> > headers) {
    return _with_headers<std::initializer_list< std::pair<std::string_view, std::string_view> > >(headers);
}

// URL params, automatically percent-escape reserved characters
template<class T> client::request&& client::request::_with_params(T params) {
    _valid_state(false);
    if (_d->path.empty())
        throw bad_state("cannot set params before setting path");
    if (_d->path_asterisk)
        throw bad_state("cannot set params for asterisk path");
    if (_d->query_set)
        throw bad_state("query segment already set");

    // create query str from these params
    std::string query_str = "?";
    bool first = true;

    for (const auto &param : params) {
        if (first) {
            first = false;
        } else {
            // & delimiter
            query_str += '&';
        }

        // key
        query_str += uri::percent_encode(param.first);

        // = delimiter
        query_str += '=';

        // value
        query_str += uri::percent_encode(param.second);
    }

    _d->path += query_str;
    _d->query_set = true;

    return std::move(*this);
}

client::request&& client::request::with_params(const std::map<std::string, std::string> &params) {
    return _with_params<const std::map<std::string, std::string>&>(params);
}

// multiple instances of the same param allowed
client::request&& client::request::with_params(const std::vector< std::pair<std::string, std::string> > &params) {
    return _with_params<const std::vector< std::pair<std::string, std::string> >&>(params);
}

// multiple instances of the same param allowed
client::request&& client::request::with_params(std::initializer_list< std::pair<std::string_view, std::string_view> > params) {
    return _with_params<std::initializer_list< std::pair<std::string_view, std::string_view> > >(params);
}

client::request&& client::request::allow_invalid_cert(bool value) {
    _valid_state(false);
    _d->allow_invalid_cert = value;
    return std::move(*this);
}

client::request&& client::request::allow_unsafe_ports(bool value) {
    _valid_state(false);
    _d->allow_unsafe_ports = value;
    return std::move(*this);
}

client::request&& client::request::with_content_type(std::string_view type) {
    _valid_state(false);
    _with_header_trust_name("content-type"s, type);
    return std::move(*this);
}

template<class T> client::request&& client::request::_with_body_str(T body) {
    _valid_state(false);
    _d->body_str = body;
    _d->body_set = true;
    return std::move(*this);
}

client::request&& client::request::with_body_str(std::string_view body) {
    return _with_body_str<std::string_view>(body);
}

client::request&& client::request::with_body_str(const std::string &body) {
    return _with_body_str<const std::string&>(body);
}

client::request&& client::request::with_body_str(std::string &&body) {
    return _with_body_str<std::string&&>(std::move(body));
}

client::request&& client::request::with_body_str(const char *body) {
    return _with_body_str<const char*>(body);
}

client::request&& client::request::with_body_str(const char *body, size_t length) {
    _valid_state(false);
    _d->body_str.assign(body, length);
    _d->body_set = true;
    return std::move(*this);
}

client::request&& client::request::recv_as_stream() {
    _valid_state(false);
    _d->recv_mode = RECV_STREAM;
    return std::move(*this);
}

client::request&& client::request::recv_to_str() {
    return recv_to_str(STRTB_HTTP_RECV_TO_STR_MAX_LEN_DEFAULT);
}

client::request&& client::request::recv_to_str(size_t max_len) {
    _valid_state(false);
    _d->recv_mode = RECV_STR;
    _d->recv_max_len = max_len;
    return std::move(*this);
}

client::request&& client::request::with_shutdown_controller(shutdown_controller &ctrl) {
    // NOTE: the HTTP client will automatically pass the controller to the response object
    _valid_state(false);
    std::lock_guard<std::mutex> guard(_d->lock);

    if (_d->shutdown_ctrl)
        shutdown_controllable_throw_already_attached();

    _d->shutdown_ctrl_state = shutdown_controllable_attach(ctrl);
    _d->shutdown_ctrl = &ctrl;
    // NOTE: skipping call to shutdown cause this can only run before submission

    return std::move(*this);
}

void client::request::detach_shutdown_controller() {
    if (!_d)
        return;
    _valid_state(false);

    shutdown_controller *p;

    {
        std::lock_guard<std::mutex> guard(_d->lock);
        if (!_d->shutdown_ctrl)
            return;

        p = _d->shutdown_ctrl;
        _d->shutdown_ctrl = nullptr;
        _d->shutdown_ctrl_state = false;
    }

    shutdown_controllable_detach(p);
}

void client::request::shutdown_controllable_signal(bool state) {
    assert(_d);
    std::lock_guard<std::mutex> guard(_d->lock);
    // ignore mid-detach
    if (!_d->shutdown_ctrl)
        return;

    _d->shutdown_ctrl_state = state;
    if (state)
        _shutdown();
}

void client::request::_ready_to_send() {
    _valid_state(false);
    if (_d->host.empty())
        throw std::invalid_argument("invalid request: host not set");
    if (_d->path.empty())
        throw std::invalid_argument("invalid request: path not set");
}

void client::request::_send() {
    if (_d->shutdown_ctrl_state)
        throw in_shutdown_state("http request was shut down");
    _d->handler_queued = request_handler::main->send(_d);
    _d->handler_used = true;
}

client::response client::request::_get_response(std::unique_lock<std::mutex> &lock) {
    // wait for response or exception to be returned
    while (_d->handler_response.empty() && !_d->handler_exception)
        _d->cv.wait(lock);

    if (_d->handler_exception) {
        // clean up and rethrow
        _d->handler_used = false;
        std::exception_ptr ptr = std::move(_d->handler_exception);
        _d->handler_exception = nullptr;
        _d->handler_response.clear();
        std::rethrow_exception(ptr);
    }

    if (!_d->handler_response.empty()) {
        // clean up and return response
        _d->handler_used = false;
        return std::move(_d->handler_response);
    }

    throw std::logic_error("reached unreachable part in "s + __func__);
}

client::request&& client::request::send_async() {
    _ready_to_send();
    std::lock_guard<std::mutex> guard(_d->lock);
    _send();
    return std::move(*this);
}

client::response client::request::send() {
    _ready_to_send();
    std::unique_lock<std::mutex> lock(_d->lock);
    _send();
    return _get_response(lock);
}

client::response client::request::get_response() {
    _valid_state(true);
    std::unique_lock<std::mutex> lock(_d->lock);
    if (!_d->handler_used)
        throw bad_state(__func__ + " can only be used when sent to the request handler"s);
    return _get_response(lock);
}

bool client::request::empty() {
    return _d == nullptr;
}

client::request&& client::request::treat_as_safe() {
    _valid_state(false);
    _d->method_safe = true;
    _d->method_idempotent = true;
    return std::move(*this);
}

client::request&& client::request::treat_as_idempotent() {
    _valid_state(false);
    _d->method_safe = false;
    _d->method_idempotent = true;
    return std::move(*this);
}

client::request&& client::request::treat_as_non_idempotent() {
    _valid_state(false);
    _d->method_safe = false;
    _d->method_idempotent = false;
    return std::move(*this);
}

client::request&& client::request::with_auth_bearer(std::string_view token) {
    _valid_state(false);
    _with_header_trust_name("authorization"s, "Bearer " + token);
    return std::move(*this);
}

client::request&& client::request::with_auth_basic(std::string_view username, std::string_view password) {
    _valid_state(false);
    /* check username and password for invalid characters:
     * neither can contain control characters,
     * username also can't contain a colon, as it's used
     * as a separator between the username and password
     */
    for (char c : username)
        if (is_ctl(c) || c == ':')
            throw std::invalid_argument("username contains invalid character " + char_escape(c));
    for (char c : password)
        if (is_ctl(c))
            throw std::invalid_argument("password contains invalid character " + char_escape(c));

    // encode and set header
    _with_header_trust_name("authorization"s, "Basic " + base64_encode(username + ":" + password));

    return std::move(*this);
}

client::request&& client::request::with_auth_basic(std::string_view userinfo) {
    _valid_state(false);
    // this expects the userinfo segment from a URL

    bool has_colon = false;

    /* check username and password for invalid characters and form:
     * neither can contain control characters,
     * at least one colon to separate username from password
     */
    for (char c : userinfo) {
        if (is_ctl(c))
            throw std::invalid_argument("userinfo contains invalid character " + char_escape(c));
        else if (c == ':')
            has_colon = true;
    }

    if (!has_colon)
        throw std::invalid_argument("missing colon ':' separator between username and password");

    // encode and set header
    _with_header_trust_name("authorization"s, "Basic " + base64_encode(userinfo));

    return std::move(*this);
}

template<class T> client::request&& client::request::_upgrade(T protocols) {
    _valid_state(false);

    try {
        _d->upgrade.clear();
        for (const auto &p : protocols)
            _upgrade(std::string_view(p));
    } catch (...) {
        _d->upgrade.clear();
        throw;
    }

    return std::move(*this);
}

void client::request::_upgrade(std::string_view protocol) {
    auto ret = parse_product_or_protocol(protocol);
    if (!ret.valid || ret.to != protocol.length())
        throw std::invalid_argument("invalid syntax: " + string_escape(protocol));

    _d->upgrade.push_back(std::move(ret.pr));
}

client::request&& client::request::upgrade(std::string_view protocol) {
    _valid_state(false);
    _d->upgrade.clear();
    _upgrade(protocol);
    return std::move(*this);
}

client::request&& client::request::upgrade(const std::vector<std::string> &protocols) {
    return _upgrade<const std::vector<std::string> &>(protocols);
}

client::request&& client::request::upgrade(const std::vector<std::string_view> &protocols) {
    return _upgrade<const std::vector<std::string_view>&>(protocols);
}

client::request&& client::request::upgrade(std::initializer_list<std::string_view> protocols) {
    return _upgrade<std::initializer_list<std::string_view> >(protocols);
}

client::response::response(client *c) {
    _d = new data;
    _d->c = c;
}

client::response::~response() {
    clear();
}

void client::response::_move(response &&other) {
    // NOTE: clearing this object's data must be done by caller if necessary

    // temporarily detach any shutdown controller
    bool ctrl_attached = false;
    if (other._d && other._d->shutdown_ctrl) {
        ctrl_attached = true;
        other.shutdown_controllable_detach(other._d->shutdown_ctrl);
    }

    // move over the data struct
    _d = other._d;
    other._d = nullptr;

    // reattach the shutdown controller
    if (ctrl_attached) {
        std::lock_guard<std::mutex> guard(_d->lock);
        _d->shutdown_ctrl_state = shutdown_controllable_attach(*_d->shutdown_ctrl);
        if (_d->shutdown_ctrl_state)
            _shutdown();
    }
}

client::response::response(response &&other) {
    _move(std::move(other));
}

client::response& client::response::operator=(response &&other) {
    clear();
    _move(std::move(other));
    return *this;
}

http_version client::response::version() const {
    _verify_data();
    return _d->version;
}

int client::response::status() const {
    _verify_data();
    return _d->status;
}

const std::string& client::response::status_message() const {
    _verify_data();
    return _d->status_message;
}

const std::string& client::response::header(std::string_view name) const {
    _verify_data();
    return _d->headers.get_field(name);
}

const std::map<std::string, std::string>& client::response::headers() const {
    _verify_data();
    return _d->headers.fields;
}

const std::vector<std::string>& client::response::headers_set_cookie() const {
    _verify_data();
    return _d->headers.fields_set_cookie;
}

const std::string& client::response::trailer(std::string_view name) const {
    _verify_data();
    return _d->trailers.get_field(name);
}

const std::map<std::string, std::string>& client::response::trailers() const {
    _verify_data();
    return _d->trailers.fields;
}

std::string_view client::response::recv_body() {
    return recv_body(0);
}

std::string_view client::response::recv_body(size_t max_len) {
    _verify_data();
    _verify_recv_mode(RECV_STREAM, __func__);
    if (!_d->c)
        return std::string_view();

    return _d->c->recv_body(max_len);
}

std::string client::response::body_str() {
    _verify_data();
    _verify_recv_mode(RECV_STR, __func__);
    return std::move(_d->body_str);
}

// make sure a data struct exists before each data access
void client::response::_verify_data() const {
    if (!_d)
        throw bad_state("no response assigned");
}

// make sure the caller (of the "parent" function) used the correct receive mode
void client::response::_verify_recv_mode(recv_mode_enum wanted, const char *f_name) const {
    if (_d->recv_mode != wanted) {
        std::string name;

        switch (_d->recv_mode) {
        case RECV_STREAM:
            name = "as_stream";
            break;

        case RECV_STR:
            name = "to_str";
            break;

        default:
            name = "invalid value";
        }

        throw std::logic_error("this function ("s + f_name + ") cannot handle "
                               "the requested receive mode (" + name + ")");
    }
}

void client::response::_cancel() {
    if (_d->c) {
        _d->c->cancel_response();
        _d->c = nullptr;
    }
}

void client::response::cancel() {
    if (_d) {
        std::lock_guard<std::mutex> guard(_d->lock);
        _cancel();
    }
}

void client::response::clear() {
    if (_d) {
        if (_d->shutdown_ctrl)
            shutdown_controllable_detach(_d->shutdown_ctrl);
        _cancel();
        delete _d;
        _d = nullptr;
    }
}

void client::response::_shutdown() {
    if (_d->c)
        _d->c->shutdown_response();
}

void client::response::attach_shutdown_controller(shutdown_controller &ctrl) {
    _verify_data();
    std::lock_guard<std::mutex> guard(_d->lock);
    if (_d->shutdown_ctrl)
        shutdown_controllable_throw_already_attached();

    _d->shutdown_ctrl_state = shutdown_controllable_attach(ctrl);
    _d->shutdown_ctrl = &ctrl;

    if (_d->shutdown_ctrl_state)
        _shutdown();
}

void client::response::detach_shutdown_controller() {
    if (!_d)
        return;

    shutdown_controller *p;

    {
        std::lock_guard<std::mutex> guard(_d->lock);
        if (!_d->shutdown_ctrl)
            return;

        p = _d->shutdown_ctrl;
        _d->shutdown_ctrl = nullptr;
        _d->shutdown_ctrl_state = false;
    }

    shutdown_controllable_detach(p);
}

void client::response::shutdown_controllable_signal(bool state) {
    assert(_d);
    std::lock_guard<std::mutex> guard(_d->lock);
    // ignore mid-detach
    if (!_d->shutdown_ctrl)
        return;

    _d->shutdown_ctrl_state = state;
    if (state)
        _shutdown();
}

bool client::response::empty() {
    return _d == nullptr;
}

/* clears status and header data; used by http::client to reuse the object
 * after processing an 1xx informational response
 */
void client::response::soft_clear() {
    assert(_d);
    _d->version = version();
    _d->status = 0;
    _d->status_message.clear();
    _d->headers.clear();
}

std::variant<std::monostate, networking::tcp_client, networking::tcp_client_ssl> client::response::socket() {
    _verify_data();
    return std::move(_d->socket);
}

const std::vector<product>& client::response::upgrade() const {
    _verify_data();
    return _d->upgrade;
}

// request creation shortcuts
// using a #define here isn't the prettiest code, but it removes the need for a lot of copy-pasting
// can't use a template cause I gotta define different function names, too

#define STRTB_HTTP_CLIENT_DEFINE_REQUEST_CREATOR(func_name, method_str) \
    client::request func_name() { \
        return client::request(method_str); \
    } \
    \
    client::request func_name(std::string_view url) { \
        return client::request(method_str).with_url(url); \
    } \
    \
    client::request func_name(bool https, std::string_view hostname) { \
        return client::request(method_str).with_host(https, hostname); \
    } \
    \
    client::request func_name(bool https, std::string_view hostname, unsigned int port) { \
        return client::request(method_str).with_host(https, hostname, port); \
    }

STRTB_HTTP_CLIENT_DEFINE_REQUEST_CREATOR(get, "GET");
STRTB_HTTP_CLIENT_DEFINE_REQUEST_CREATOR(head, "HEAD");
STRTB_HTTP_CLIENT_DEFINE_REQUEST_CREATOR(post, "POST");
STRTB_HTTP_CLIENT_DEFINE_REQUEST_CREATOR(put, "PUT");
STRTB_HTTP_CLIENT_DEFINE_REQUEST_CREATOR(options, "OPTIONS");
STRTB_HTTP_CLIENT_DEFINE_REQUEST_CREATOR(delete_m, "DELETE");

incomplete_upload::incomplete_upload(client::response &&rs) :
    exception("upload aborted by an early "s + std::to_string(rs.status()) +
              " (" + rs.status_message() + ") response"),
    _rs(std::move(rs)) {}

/* fake copy constructor (moves response object) to avoid issues
 * with implementations of std::current_exception() that copy
 */
incomplete_upload::incomplete_upload(incomplete_upload &other) :
    exception(other), _rs(std::move(other._rs)) {}

incomplete_upload::incomplete_upload(incomplete_upload &&other) :
    exception(std::move(other)), _rs(std::move(other._rs)) {}

client::response& incomplete_upload::response() {
    return _rs;
}

}
