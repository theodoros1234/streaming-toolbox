#ifndef STRTB_NETWORKING_TCP_SOCKET_SSL_H
#define STRTB_NETWORKING_TCP_SOCKET_SSL_H

#include "tcp_socket.h"
#include <openssl/ssl.h>

namespace strtb::networking {

class tcp_socket_ssl : public tcp_socket {
protected:
    std::mutex _lock_rw;
    bool _lock_rw_needed = true;
    SSL* _ssl = nullptr;
public:
    tcp_socket_ssl();
    ~tcp_socket_ssl();
    ssize_t recv(size_t max_len);
    ssize_t send(const char* buf, size_t len);
    bool shutdown_gracefully();
    void close();
};

}

#endif // STRTB_NETWORKING_TCP_SOCKET_SSL_H
