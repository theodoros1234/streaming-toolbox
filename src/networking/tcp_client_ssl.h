#ifndef STRTB_NETWORKING_TCP_CLIENT_SSL_H
#define STRTB_NETWORKING_TCP_CLIENT_SSL_H

#include "tcp_client.h"
#include "tcp_socket_ssl_thread.h"
#include <openssl/ssl.h>

namespace strtb::networking {

class tcp_client_ssl : public tcp_client {
protected:
    SSL* _ssl = nullptr;
    tcp_socket_ssl_thread _thread;
public:
    tcp_client_ssl();
    tcp_client_ssl(size_t recv_buffer_size);
    ~tcp_client_ssl();
    void connect(const char* address, uint16_t port, time_t timeout = 30);
    void connect(const char* address, uint16_t port, bool allow_abrupt_shutdown = false, bool verify_certificate = true, SSL_CTX* ssl_context = nullptr, time_t timeout = 30);
    void connect(const std::string& address, uint16_t port, bool allow_abrupt_shutdown = false, bool verify_certificate = true, SSL_CTX* ssl_context = nullptr, time_t timeout = 30);
    ssize_t recv(size_t max_len);
    ssize_t send(const char* buf, size_t len);
    void shutdown_gracefully();
    void close();
    SSL* ssl() const;
};

}

#endif // STRTB_NETWORKING_TCP_CLIENT_SSL_H
