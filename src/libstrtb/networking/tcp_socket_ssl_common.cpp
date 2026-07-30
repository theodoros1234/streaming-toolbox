#include "tcp_socket_ssl_common.h"
#include "exceptions.h"
#include "sigpipe_suppressor.h"

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

}
