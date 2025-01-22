#ifndef STRTB_NETWORKING_TCP_CLIENT_H
#define STRTB_NETWORKING_TCP_CLIENT_H

#include "tcp_socket.h"

namespace strtb::networking {

class tcp_client : public tcp_socket {
private:
    std::string _remote_ip;
    int _remote_port = 0;
public:
    tcp_client();
    ~tcp_client() = default;
    void connect(const char* address, uint16_t port, bool reconnect = false);
    void connect(const std::string& address, uint16_t port, bool reconnect = false);
    bool close();
    std::string remote_ip();
    int remote_port();
};

}

#endif // STRTB_NETWORKING_TCP_CLIENT_H
