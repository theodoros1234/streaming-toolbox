#ifndef STRTB_NETWORKING_TCP_SERVER_H
#define STRTB_NETWORKING_TCP_SERVER_H

#include <cstdint>
#include <mutex>
#include <string>
#include "../common/deregistration_interface.h"
#include "exceptions.h"

namespace strtb::networking {

struct tcp_server_platform_specific;

class tcp_server {
private:
    tcp_server_platform_specific* _pl;
    std::mutex _lock;
    std::string _ip;
    int _ip_family, _port, _backlog, _max_active;

public:
    tcp_server();
    ~tcp_server();
    void listen(const char* address, uint16_t port, bool reconnect = false, int backlog = 128, int max_active = 64);
    void listen(const std::string& address, uint16_t port, bool reconnect = false, int backlog = 128, int max_active = 64);
    void* accept();
    bool shutdown_incoming();
    bool shutdown_connected();
    bool close();
};

}

#endif // STRTB_NETWORKING_TCP_SERVER_H
