#ifndef STRTB_NETWORKING_TCP_CLIENT_SSL_H
#define STRTB_NETWORKING_TCP_CLIENT_SSL_H

#include "tcp_client.h"
#include "tcp_socket_ssl_thread.h"
#include <openssl/ssl.h>

namespace strtb::networking {

class tcp_client_ssl : public tcp_client {
private:
    bool _thread_assisted = false;

protected:
    SSL* _ssl = nullptr;
    tcp_socket_ssl_thread _thread;
    virtual size_t _recv(size_t len);
    virtual void _send(const char* buf, size_t len);
    virtual bool _available();

public:
    tcp_client_ssl(bool buffered_send = false, bool thread_assisted = false,
                   size_t buffer_size = STRTB_NETWORKING_RECV_BUFFER_SIZE_DEFAULT_SSL);
    ~tcp_client_ssl();
    void connect(const char* address, uint16_t port, bool allow_abrupt_shutdown = false, bool verify_certificate = true, SSL_CTX* ssl_context = nullptr, time_t timeout = 30);
    void connect(const std::string& address, uint16_t port, bool allow_abrupt_shutdown = false, bool verify_certificate = true, SSL_CTX* ssl_context = nullptr, time_t timeout = 30);
    void shutdown_gracefully();
    void close();
    SSL* ssl() const;
    bool thread_assisted() const;
    void thread_assist_enable();
    void thread_assist_disable();
};

}

#endif // STRTB_NETWORKING_TCP_CLIENT_SSL_H
