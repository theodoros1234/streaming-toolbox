#ifndef STRTB_NETWORKING_TCP_SERVER_CONNECTION_H
#define STRTB_NETWORKING_TCP_SERVER_CONNECTION_H

#include <string>
#include "tcp_socket.h"
#include "../common/deregistration_interface.h"

namespace strtb::networking {

class tcp_server;

class tcp_server_connection : public tcp_socket {
protected:
    strtb::common::deregistration_interface<class tcp_server_connection*> *_parent = nullptr;
    const std::string _server_ip, _remote_ip;
    const int _server_port = 0, _remote_port = 0;
    friend tcp_server;
    tcp_server_connection(strtb::common::deregistration_interface<class tcp_server_connection*> *parent,
                          size_t recv_buffer_size,
                          int fd,
                          std::string server_ip,
                          int server_port,
                          std::string remote_ip,
                          int remote_port);
public:
    virtual ~tcp_server_connection();
    void close();
    const std::string& server_ip() const;
    const std::string& remote_ip() const;
    int server_port() const;
    int remote_port() const;
};

}

#endif // STRTB_NETWORKING_TCP_SERVER_CONNECTION_H
