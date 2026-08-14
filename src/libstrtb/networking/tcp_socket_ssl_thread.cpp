#include "tcp_socket_ssl_thread.h"
#include "../logging.h"
#include "sigpipe_suppressor.h"
#include <sys/eventfd.h>
#include <poll.h>
#include "exceptions.h"
#include <assert.h>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <utility>

using namespace strtb::networking;

static strtb::logging::source log("SSL Helper Thread", false);

tcp_socket_ssl_thread::tcp_socket_ssl_thread() {}

tcp_socket_ssl_thread::~tcp_socket_ssl_thread() {
    if (_t.joinable()) {
        log.put(strtb::logging::WARNING, {"Object destroyed while thread was still open or not properly closed. Stopping thread, but this could cause a crash. Please report this bug."});
        stop();
    }
    release();
}

void tcp_socket_ssl_thread::start(int sock, SSL* ssl) {
    if (_t.joinable())
        throw internal_error("SSL helper thread already started", 0);
    if (_eventfd == -1) {
        _eventfd = eventfd(0, 0);
        if (_eventfd == -1)
            throw internal_error("failed to create internal synchronization mechanism: " + std::string(std::strerror(errno)), errno);
    }
    if (sock == -1)
        throw std::invalid_argument("invalid socket");
    if (ssl == nullptr)
        throw std::invalid_argument("SSL object can't be null");

    // Make socket non-blocking
    int flags = fcntl(sock, F_GETFL);
    if (flags == -1)
        throw internal_error(errno);
    flags |= O_NONBLOCK;
    if (fcntl(sock, F_SETFL, flags))
        throw internal_error(errno);

    _sock  = sock;
    _ssl = ssl;
    _requested_read = false;
    _requested_write = false;
    _requested_available = false;
    _requested_shutdown = false;
    _requested_close = false;
    _shutdown_sent = false;
    _errno_ssl = 0;
    _errno_syscall = 0;

    try {
        _thread_active = true;
        _t = std::thread(&tcp_socket_ssl_thread::thread_loop, this);
    } catch (std::exception& e) {
        _sock = -1;
        _ssl = nullptr;
        _thread_active = false;
        throw internal_error("SSL helper thread could not be started: " + std::string(e.what()), 0);
    } catch (...) {
        _sock = -1;
        _ssl = nullptr;
        _thread_active = false;
        throw;
    }
}

void tcp_socket_ssl_thread::stop() {
    if (!_t.joinable())
        throw internal_error("SSL helper thread not running", 0);

    {
        std::lock_guard<std::mutex> guard(_lock);
        if (_thread_active && _eventfd != -1) {
            _requested_close = true;
            uint64_t u = 1;
            write(_eventfd, &u, sizeof(uint64_t));
        }
    }

    _t.join();
    _ssl = nullptr;
    _sock = -1;
    _errno_ssl = 0;
    _errno_syscall = 0;
}

size_t tcp_socket_ssl_thread::recv(char* buffer, size_t length) {
    std::unique_lock<std::mutex> guard(_lock);

    if (!_thread_active)
        _decide_exception();

    if (_requested_read)
        throw bad_threading("recv() or available() seem to be waiting already from another thread");

    _buffer_read = buffer;
    _length_read = length;

    // Wakeup SSL thread
    _requested_read = true;
    uint64_t u = 1;
    if (write(_eventfd, &u, sizeof(uint64_t)) != sizeof(uint64_t))
        throw internal_error("error in internal synchronization mechanism: " + std::string(std::strerror(errno)), errno);

    // Wait to receive data
    _successful_read = false;
    _cv_read.wait(guard);

    if (!_successful_read)
        _decide_exception();

    return _length_read;
}

bool tcp_socket_ssl_thread::available() {
    std::unique_lock<std::mutex> guard(_lock);

    if (!_thread_active)
        _decide_exception();

    if (_requested_read)
        throw bad_threading("recv() or available() seem to be waiting already from another thread");

    // Wakeup SSL thread
    _requested_read = true;
    _requested_available = true;
    uint64_t u = 1;
    if (write(_eventfd, &u, sizeof(uint64_t)) != sizeof(uint64_t))
        throw internal_error("error in internal synchronization mechanism: " + std::string(std::strerror(errno)), errno);

    // Wait to receive data
    _successful_read = false;
    _cv_read.wait(guard);

    if (!_successful_read)
        _decide_exception();

    return _available;
}

void tcp_socket_ssl_thread::send(const char* buffer, size_t length) {
    std::unique_lock<std::mutex> guard(_lock);

    if (length <= 0)
        throw std::invalid_argument("send length must be positive");

    if (!_thread_active)
        _decide_exception();

    if (_requested_write)
        throw bad_threading("send() or shutdown_gracefully() seem to be already waiting from another thread");

    if (_requested_shutdown)
        throw connection_closed("the send side of the SSL connection was already shutdown", 0);

    _buffer_write = buffer;
    _length_write = length;

    // Wakeup SSL thread
    _requested_write = true;
    uint64_t u = 1;
    if (write(_eventfd, &u, sizeof(uint64_t)) != sizeof(uint64_t))
        throw internal_error("error in internal synchronization mechanism: " + std::string(std::strerror(errno)), errno);

    // Wait to send data
    _successful_write = false;
    _cv_write.wait(guard);

    if (!_successful_write)
        _decide_exception();
}

void tcp_socket_ssl_thread::shutdown_gracefully() {
    std::unique_lock<std::mutex> guard(_lock);

    if (!_thread_active)
        _decide_exception();

    if (_requested_write)
        throw bad_threading("send() or shutdown_gracefully() seem to be already waiting from another thread");

    // Wakeup SSL thread
    _requested_write = true;
    _requested_shutdown = true;
    uint64_t u = 1;
    if (write(_eventfd, &u, sizeof(uint64_t)) != sizeof(uint64_t))
        throw internal_error("error in internal synchronization mechanism: " + std::string(std::strerror(errno)), errno);

    // Wait to send shutdown
    _cv_write.wait(guard);

    if (_shutdown_sent)
        return;
    else
        _decide_exception();
}

void tcp_socket_ssl_thread::thread_loop() {
    bool poll_read = false, poll_write = false, incomplete_write = false;
    int ret;
    /* TODO: Since this is a separate thread now, the SIGPIPE suppressor could be replaced
     * with just permanently changing the current thread's sigmask instead. */
    sigpipe_suppressor shutup;

    struct pollfd p[2];
    p[0].fd = _sock;
    p[1].fd = _eventfd;
    p[1].events = POLLIN;

    while (_thread_active) {
        p[0].events = 0;
        if (poll_read)
            p[0].events |= POLLIN;
        if (poll_write)
            p[0].events |= POLLOUT;
        poll(p, 2, -1);
        poll_read = false;
        poll_write = false;

        std::lock_guard<std::mutex> guard(_lock);

        if (p[1].revents & POLLIN) {
            uint64_t u;
            if (read(_eventfd, &u, sizeof(uint64_t)) != sizeof(uint64_t)) {
                _errno_syscall = errno;
                close(_eventfd);
                _eventfd = -1;
                _thread_active = false;
                break;
            }
        } else if (p[1].revents & POLLHUP) {
            // Can't continue if eventfd fails in some way
            close(_eventfd);
            _eventfd = -1;
            _thread_active = false;
            break;
        }

        if (_requested_close) {
            _thread_active = false;
            break;
        }

        if (_requested_write) {
            incomplete_write = false;

            if (_requested_shutdown) {
                ret = SSL_shutdown(_ssl);
                if (ret >= 0) {
                    _shutdown_sent = true;
                    _requested_write = false;
                    _cv_write.notify_one();
                }

            } else {
                /* TODO: maybe replace with SSL_write_ex and allow partial writes,
                 * or only write one record every time, so that it doesn't block
                 * the read side when sending a lot of data at a time.
                 * This thread will handle sending everything after partial writes.
                 */
                ret = SSL_write(_ssl, _buffer_write, _length_write);
                if (ret > 0) {
                    _length_write = ret;
                    _successful_write = true;
                    _requested_write = false;
                    _cv_write.notify_one();
                }
            }

            // Handle error from either SSL_write or SSL_shutdown
            if (_requested_write) {
                int errno_ssl = SSL_get_error(_ssl, ret);
                switch (errno_ssl) {
                case SSL_ERROR_WANT_READ:
                    poll_read = true;
                    break;

                case SSL_ERROR_WANT_WRITE:
                    poll_write = true;
                    incomplete_write = true;
                    break;

                case SSL_ERROR_SYSCALL:
                    _errno_syscall = errno;
                    [[fallthrough]];
                default:
                    _errno_ssl = errno_ssl;
                    _thread_active = false;
                }
            }
        }

        if (!_thread_active)
            break;

        if (_requested_read && !incomplete_write) {
            if (_requested_available) {
                // Check if incoming data (or shutdown/error) is available
                char b;
                ret = SSL_peek(_ssl, &b, 1);
                if (ret > 0) {
                    _available = true;
                    _successful_read = true;
                    _requested_read = false;
                    _requested_available = false;
                    _cv_read.notify_one();
                } else {
                    int errno_ssl = SSL_get_error(_ssl, ret);
                    switch (errno_ssl) {
                    case SSL_ERROR_WANT_READ:
                    case SSL_ERROR_WANT_WRITE:
                    case SSL_ERROR_ZERO_RETURN:
                        _available = errno_ssl == SSL_ERROR_ZERO_RETURN;
                        _successful_read = true;
                        _requested_read = false;
                        _requested_available = false;
                        _cv_read.notify_one();
                        break;

                    case SSL_ERROR_SYSCALL:
                        _errno_syscall = errno;
                        [[fallthrough]];
                    default:
                        _errno_ssl = errno_ssl;
                        _thread_active = false;
                    }
                }

            } else {
                // Read data
                ret = SSL_read(_ssl, _buffer_read, _length_read);
                if (ret > 0) {
                    _length_read = ret;
                    _successful_read = true;
                    _requested_read = false;
                    _cv_read.notify_one();
                } else {
                    int errno_ssl = SSL_get_error(_ssl, ret);
                    switch (errno_ssl) {
                    case SSL_ERROR_WANT_READ:
                        poll_read = true;
                        break;

                    case SSL_ERROR_WANT_WRITE:
                        poll_write = true;
                        break;

                    case SSL_ERROR_ZERO_RETURN:
                        _length_read = 0;
                        _successful_read = true;
                        _requested_read = false;
                        _cv_read.notify_one();
                        break;

                    case SSL_ERROR_SYSCALL:
                        _errno_syscall = errno;
                        [[fallthrough]];
                    default:
                        _errno_ssl = errno_ssl;
                        _thread_active = false;
                    }
                }
            }
        }

        // When expanding this loop in the future, don't forget to check and break if !_thread_active
    }
    assert(_thread_active == false);

    std::lock_guard<std::mutex> guard(_lock);
    _cv_read.notify_all();
    _cv_write.notify_all();
}

void tcp_socket_ssl_thread::_decide_exception() {
    if (_eventfd == -1)
        throw internal_error("error in internal synchronization mechanism", _errno_syscall);

    if (_errno_ssl == 0 && !_thread_active)
        throw connection_closed("Connection closed", 0);

    switch (_errno_ssl) {
    case SSL_ERROR_SSL:
        throw connection_error_ssl("SSL/TLS protocol/connection error", _errno_ssl);
    case SSL_ERROR_SYSCALL:
        switch (_errno_syscall) {
        case ECONNREFUSED:
            throw connection_error(errno);

        case EPIPE:
        case ENOTCONN:
            throw connection_closed(errno);

        default:
            throw internal_error(errno);
        }
    default:
        throw internal_error_ssl("SSL/TLS internal error (error code " + std::to_string(_errno_ssl) + ")", _errno_ssl);
    }
}

void tcp_socket_ssl_thread::release() {
    // release internal resources (they'll be automatically recreated if object is reused)
    assert(!_t.joinable());
    if (_eventfd != -1) {
        ::close(_eventfd);
        _eventfd = -1;
    }
}

void tcp_socket_ssl_thread::steal_event_signaller(tcp_socket_ssl_thread &other) {
    assert(!_t.joinable());
    assert(!other._t.joinable());
    if (_eventfd == -1)
        _eventfd = std::exchange(other._eventfd, -1);
}
