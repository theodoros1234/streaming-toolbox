#ifndef STRTB_NETWORKING_TCP_SERVER_SSL_H
#define STRTB_NETWORKING_TCP_SERVER_SSL_H

#include "tcp_server.h"
#include "tcp_server_connection_ssl.h"
#include <openssl/ssl.h>

namespace strtb::networking {

class tcp_server_ssl : public tcp_server {
private:
    SSL_CTX* _ctx;
    bool _ctx_caller_provided = false;
    bool _thread_assisted = true;

protected:
    tcp_server_connection* _new_connection(const bound_port& server, int sock, std::string remote_ip, int remote_port);

public:
    tcp_server_ssl(SSL_CTX* ctx = nullptr, bool buffered_send = false, bool thread_assisted = false,
                   size_t buffer_size = STRTB_NETWORKING_RECV_BUFFER_SIZE_DEFAULT_SSL);
    ~tcp_server_ssl();
    SSL_CTX* ssl_ctx() const;
    tcp_server_connection_ssl* accept();
    bool thread_assisted() const;   // applies to individual connection sockets
};

}

#endif // STRTB_NETWORKING_TCP_SERVER_SSL_H
