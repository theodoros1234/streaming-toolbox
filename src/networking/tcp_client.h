#ifndef STRTB_NETWORKING_TCP_CLIENT_H
#define STRTB_NETWORKING_TCP_CLIENT_H

#include <time.h>

#include "tcp_socket.h"

namespace strtb::networking {

class tcp_client : public tcp_socket {
protected:
    std::string _remote_ip;
    int _remote_port = 0;
    bool _connecting = false, _cancel_sent = false, _connect_restrict = false;

    // Platform-specific
#ifdef __linux__
    int _event;
#endif

public:
    tcp_client();
    ~tcp_client();
    void connect(const char* address, uint16_t port, time_t timeout = 30);
    void connect(const std::string& address, uint16_t port, time_t timeout = 30);
    void cancel_connect();
    void reset();
    void close();
    std::string remote_ip();
    int remote_port();
    bool is_connecting();
};

}

#endif // STRTB_NETWORKING_TCP_CLIENT_H
