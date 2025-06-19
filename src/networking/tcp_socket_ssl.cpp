#include "tcp_socket_ssl.h"
#include <stdexcept>
#include <assert.h>
#include <sys/socket.h>
#include "../logging/logging.h"

using namespace strtb;
using namespace strtb::networking;

static logging::source log("TCP Socket with SSL/TLS");

tcp_socket_ssl::tcp_socket_ssl() {}

tcp_socket_ssl::~tcp_socket_ssl() {
    if (_ssl) {
        log.put(logging::WARNING, {"Destructor called when socket was still open. Closing the SSL connection and the socket, but this may lead to a crash. If you're a plugin developer, make sure you call close() on the socket."});
        ::shutdown(_sock, SHUT_RDWR);
        SSL_free(_ssl);
        _ssl = nullptr;
        _sock = -1;
    }
}

ssize_t tcp_socket_ssl::recv(size_t max_len) {
    assert((_sock == -1) == (_ssl == nullptr));

    if (_ssl == nullptr)
        throw connection_closed("socket closed or hasn't been opened yet", 0);

    if (max_len == 0)
        throw std::invalid_argument("max_len cannot be 0");

    if (max_len > STRTB_NETWORKING_RECV_BUFFER_SIZE)
        max_len = STRTB_NETWORKING_RECV_BUFFER_SIZE;

    // Return from recv_line's leftovers if there are enough to cover the request
    if (max_len <= _line_leftovers) {   // TODO: I haven't properly tested this part, but it should be working
        memmove(_buffer, _buffer + _line_leftovers_pos, max_len);
        _line_leftovers_pos += max_len;
        _line_leftovers -= max_len;
        return max_len;
    }

    // Move back any leftover bytes from recv_line
    if (_line_leftovers) {
        memmove(_buffer, _buffer + _line_leftovers_pos, _line_leftovers);
        _line_leftovers = 0;
    }

    size_t readbytes = 0;
    if (_lock_rw_needed)
        _lock_rw.lock();

    try {
        errno = 0;
        if (!SSL_read_ex(_ssl, _buffer + _line_leftovers, max_len - _line_leftovers, &readbytes)) {
            int ssl_errno = SSL_get_error(_ssl, 0);
            switch (ssl_errno) {
            case SSL_ERROR_ZERO_RETURN:
                readbytes = 0;
                break;

            case SSL_ERROR_SSL:
                throw connection_error_ssl("SSL/TLS protocol/connection error", ssl_errno);

            case SSL_ERROR_SYSCALL:
                switch (errno) {
                case 0:
                    throw internal_error_ssl("SSL/TLS syscall error", 0);

                case ECONNREFUSED:
                    throw connection_error(errno);

                default:
                    throw internal_error(errno);
                }

            default:
                throw internal_error_ssl("SSL/TLS internal error", ssl_errno);
            }
        }
    } catch (...) {
        if (_lock_rw_needed)
            _lock_rw.unlock();
        throw;
    }

    if (_lock_rw_needed)
        _lock_rw.unlock();

    return readbytes;
}

ssize_t tcp_socket_ssl::send(const char* buf, size_t len) {
    assert((_sock == -1) == (_ssl == nullptr));

    if (_ssl == nullptr)
        throw connection_closed("socket closed or hasn't been opened yet", 0);

    size_t writebytes = 0;
    if (_lock_rw_needed)
        _lock_rw.lock();

    try {
        errno = 0;
        if (!SSL_write_ex(_ssl, buf, len, &writebytes)) {
            int ssl_errno = SSL_get_error(_ssl, 0);
            switch (ssl_errno) {
            case SSL_ERROR_SSL:
                throw connection_error_ssl("SSL/TLS protocol/connection error", ssl_errno);

            case SSL_ERROR_SYSCALL:
                switch (errno) {
                case 0:
                    throw internal_error_ssl("SSL/TLS syscall error", 0);

                case ECONNREFUSED:
                    throw connection_error(errno);

                case EPIPE:
                case ENOTCONN:
                    throw connection_closed(errno);

                default:
                    throw internal_error(errno);
                }

            default:
                throw internal_error_ssl("SSL/TLS internal error", ssl_errno);
            }
        }
    } catch (...) {
        if (_lock_rw_needed)
            _lock_rw.unlock();
        throw;
    }

    if (_lock_rw_needed)
        _lock_rw.unlock();
    return writebytes;
}

bool tcp_socket_ssl::shutdown_gracefully() {
    // Returns true if the server has also sent a close_notify back or false if it hasn't yet
    assert((_sock == -1) == (_ssl == nullptr));

    if (_ssl == nullptr)
        throw connection_closed("socket closed or hasn't been opened yet", 0);

    if (_lock_rw_needed)
        _lock_rw.lock();

    bool result = false;
    try {
        int shutdown_return = SSL_shutdown(_ssl);
        if (shutdown_return < 0) {
            int ssl_errno = SSL_get_error(_ssl, 0);
            switch (ssl_errno) {
            case SSL_ERROR_SSL:
                throw connection_error_ssl("SSL/TLS protocol/connection error", ssl_errno);

            case SSL_ERROR_SYSCALL:
                switch (errno) {
                case 0:
                    throw internal_error_ssl("SSL/TLS syscall error", 0);

                case ECONNREFUSED:
                    throw connection_error(errno);

                case EPIPE:
                case ENOTCONN:
                    throw connection_closed(errno);

                default:
                    throw internal_error(errno);
                }

            default:
                throw internal_error_ssl("SSL/TLS internal error", ssl_errno);
            }
        } else {
            result = shutdown_return;
        }
    } catch (...) {
        if (_lock_rw_needed)
            _lock_rw.unlock();
        throw;
    }

    if (_lock_rw_needed)
        _lock_rw.unlock();

    return result;
}

void tcp_socket_ssl::close() {
    std::lock_guard<std::recursive_mutex> guard(_lock);
    assert((_sock == -1) == (_ssl == nullptr));

    if (_ssl == nullptr)
        throw connection_closed("socket closed or hasn't been opened yet", 0);

    SSL_free(_ssl);
    _ssl = nullptr;
    _sock = -1;
}
