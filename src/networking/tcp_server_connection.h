#ifndef STRTB_NETWORKING_TCP_SERVER_CONNECTION_H
#define STRTB_NETWORKING_TCP_SERVER_CONNECTION_H

#include <string>
#include "tcp_socket.h"
#include "../common/deregistration_interface.h"

namespace strtb::networking {

class tcp_server;

class tcp_server_connection : public tcp_socket {
private:
    strtb::common::deregistration_interface<class tcp_server_connection*> *_parent = nullptr;
    std::string _server_ip, _remote_ip;
    int _server_port = 0, _remote_port = 0;
protected:
    friend tcp_server;
    void connect(int fd, std::string server_ip, int server_port, std::string remote_ip, int remote_port);
public:
    tcp_server_connection(strtb::common::deregistration_interface<class tcp_server_connection*> *parent);
    ~tcp_server_connection();
    void close();
    std::string server_ip();
    std::string remote_ip();
    int server_port();
    int remote_port();
};

}

#endif // STRTB_NETWORKING_TCP_SERVER_CONNECTION_H
