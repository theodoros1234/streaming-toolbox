#include "tcp_socket_ssl_common.h"
#include "exceptions.h"
#include "sigpipe_suppressor.h"
#include <fcntl.h>

namespace strtb::networking {

static void _ssl_decide_exception(int errno_ssl) {
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

size_t _ssl_recv(SSL *ssl, char *buf, size_t len) {
    sigpipe_suppressor shutup;
    int ret = SSL_read(ssl, buf, len);

    // handle error or EOF
    if (ret <= 0) {
        int errno_ssl = SSL_get_error(ssl, ret);
        if (errno_ssl == SSL_ERROR_ZERO_RETURN)
            return 0;
        else
            _ssl_decide_exception(errno_ssl);
    }

    return ret;
}

void _ssl_send(SSL *ssl, const char *buf, size_t len) {
    sigpipe_suppressor shutup;
    int ret = SSL_write(ssl, buf, len);

    // handle error
    if (ret <= 0) {
        int errno_ssl = SSL_get_error(ssl, ret);
        _ssl_decide_exception(errno_ssl);
    }
}

void _ssl_shutdown_gracefully(SSL *ssl) {
    sigpipe_suppressor shutup;
    int ret = SSL_shutdown(ssl);

    // handle error
    if (ret < 0) {
        int errno_ssl = SSL_get_error(ssl, ret);
        _ssl_decide_exception(errno_ssl);
    }
}

bool _ssl_available(SSL *ssl, int fd) {
    sigpipe_suppressor shutup;

    // temporarily set to non-blocking to avoid a hang
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1)
        throw internal_error(errno);
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        throw internal_error(errno);

    // check for available data (or shutdown/error)
    char b;
    int ret = SSL_peek(ssl, &b, 1);

    // restore socket flags
    if (fcntl(fd, F_SETFL, flags) == -1)
        throw internal_error(errno);

    if (ret > 0) {
        // data immediately available
        return true;
    } else {
        int error = SSL_get_error(ssl, ret);

        switch (error) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            // no ready data available
            return false;

        case SSL_ERROR_ZERO_RETURN:
            // EOF
            return true;

        default:
            _ssl_decide_exception(error);
        }
    }

    return false;
}

}
