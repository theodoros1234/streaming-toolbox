#ifndef STRTB_NETWORKING_TCP_SERVER_CONNECTION_SSL_H
#define STRTB_NETWORKING_TCP_SERVER_CONNECTION_SSL_H

#include "tcp_server_connection.h"
#include "tcp_socket_ssl_thread.h"
#include <openssl/ssl.h>

namespace strtb::networking {

class tcp_server_ssl;

class tcp_server_connection_ssl : public tcp_server_connection {
private:
    SSL* _ssl;
    tcp_socket_ssl_thread _thread;
    bool _thread_active = false;
protected:
    friend tcp_server_ssl;
    tcp_server_connection_ssl(strtb::common::deregistration_interface<class tcp_server_connection*> *parent,
                              size_t recv_buffer_size,
                              int fd,
                              std::string server_ip,
                              int server_port,
                              std::string remote_ip,
                              int remote_port,
                              SSL_CTX* ctx);
public:
    ~tcp_server_connection_ssl();
    void handshake();
    void close();
    ssize_t recv(size_t max_len);
    ssize_t send(const char* buf, size_t len);
    void shutdown_gracefully();
    SSL* ssl() const;
};

}

#endif // STRTB_NETWORKING_TCP_SERVER_CONNECTION_SSL_H
