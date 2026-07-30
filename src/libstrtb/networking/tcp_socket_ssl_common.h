#ifndef STRTB_NETWORKING_TCP_SOCKET_SSL_COMMON_H
#define STRTB_NETWORKING_TCP_SOCKET_SSL_COMMON_H

#include <cstddef>
#include <openssl/ssl.h>

namespace strtb::networking {

size_t _ssl_recv(SSL *ssl, char *buf, size_t len);
void _ssl_send(SSL *ssl, const char *buf, size_t len);
void _ssl_shutdown_gracefully(SSL *ssl);

}

#endif // STRTB_NETWORKING_TCP_SOCKET_SSL_COMMON_H
