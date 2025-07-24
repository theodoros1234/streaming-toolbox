#include "tcp_server_connection_ssl.h"
#include "../logging/logging.h"
#include <assert.h>
#include <stdexcept>

using namespace strtb::networking;

static strtb::logging::source log("TCP Socket");

tcp_server_connection_ssl::tcp_server_connection_ssl(strtb::common::deregistration_interface<class tcp_server_connection*> *parent,
                                                     size_t recv_buffer_size,
                                                     int fd,
                                                     std::string server_ip,
                                                     int server_port,
                                                     std::string remote_ip,
                                                     int remote_port,
                                                     SSL_CTX* ctx) :
    tcp_server_connection(parent, recv_buffer_size, fd, server_ip, server_port, remote_ip, remote_port) {
    _ssl = SSL_new(ctx);
    if (!_ssl)
        throw internal_error_ssl("Could not create SSL object", 0);

    BIO* bio = BIO_new(BIO_s_socket());
    if (!bio) {
        SSL_free(_ssl);
        throw internal_error_ssl("Could not create BIO object for SSL", 0);
    }

    if (BIO_set_fd(bio, _sock, BIO_CLOSE) <= 0) {
        BIO_free(bio);
        SSL_free(_ssl);
        throw internal_error_ssl("Could not attach socket to BIO object for SSL", 0);
    }

    SSL_set_bio(_ssl, bio, bio);
}

tcp_server_connection_ssl::~tcp_server_connection_ssl() {
    assert((_sock == -1) == (_ssl == nullptr));
    if (_ssl) {
        log.put(logging::WARNING, {"Destructor called when socket was still open. Closing the socket, but this may lead to a crash. If you're a plugin developer, make sure you call close() on the socket."});

        if (_thread_active) {
            _thread.stop();
            _thread_active = false;
        }

        shutdown(true, true);
        SSL_free(_ssl);
        _ssl = nullptr;
        _sock = -1;
        buffer_clear();

        if (_parent)
            _parent->deregister(this);
    }
}

void tcp_server_connection_ssl::handshake() {
    assert((_sock == -1) == (_ssl == nullptr));
    if (_ssl == nullptr)
        throw connection_closed("socket already closed", 0);
    if (_thread_active)
        throw std::logic_error("handshake() called after it was already successful");

    int ret = SSL_accept(_ssl);
    if (ret <= 0) {
        int errno_ssl = SSL_get_error(_ssl, ret);
        switch (errno_ssl) {
        case SSL_ERROR_SSL:
            throw connection_error_ssl("SSL/TLS protocol/connection error", errno_ssl);
        case SSL_ERROR_SYSCALL:
            switch (errno) {
            case ECONNREFUSED:
                throw connection_error(errno);

            case EPIPE:
            case ENOTCONN:
                throw connection_closed(errno);

            default:
                throw internal_error(errno);
            }
        default:
            throw internal_error_ssl("SSL/TLS internal error (error code " + std::to_string(errno_ssl) + ")", errno_ssl);
        }
    }

    // SSL IO helper thread
    _thread_active = true;
    _thread.start(_sock, _ssl);
}

void tcp_server_connection_ssl::close() {
    assert((_sock == -1) == (_ssl == nullptr));
    if (_ssl == nullptr)
        throw connection_closed("socket already closed", 0);

    if (_thread_active) {
        _thread.stop();
        _thread_active = false;
    }

    {
        std::lock_guard<std::recursive_mutex> guard(_lock);
        shutdown(true, true);
        SSL_free(_ssl);
        _ssl = nullptr;
        _sock = -1;
        buffer_clear();
    }

    if (_parent)
        _parent->deregister(this);
}

ssize_t tcp_server_connection_ssl::recv(size_t max_len) {
    assert((_sock == -1) == (_ssl == nullptr));

    if (_ssl == nullptr)
        throw connection_closed("socket already closed", 0);

    if (max_len == 0)
        throw std::invalid_argument("max_len cannot be 0");

    if (max_len > _buffer_size)
        max_len = _buffer_size;

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

    return _thread.recv(_buffer + _line_leftovers, max_len - _line_leftovers);
}

ssize_t tcp_server_connection_ssl::send(const char* buf, size_t len) {
    assert((_sock == -1) == (_ssl == nullptr));

    if (_ssl == nullptr)
        throw connection_closed("socket already closed", 0);

    return _thread.send(buf, len);
}

void tcp_server_connection_ssl::shutdown_gracefully() {
    // NOTE: Only call this from the sender thread. For unexpectedly cancelling the connection, use shutdown()
    assert((_sock == -1) == (_ssl == nullptr));

    if (_ssl == nullptr)
        throw connection_closed("socket already closed", 0);

    _thread.shutdown_gracefully();
}

SSL* tcp_server_connection_ssl::ssl() const {
    return _ssl;
}
