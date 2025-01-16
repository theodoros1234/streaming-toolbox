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
    std::string _ip, _ip_family;
    int _port, _backlog, _max_active;

public:
    tcp_server();
    ~tcp_server();
    void listen(const char* address, uint16_t port, bool reconnect = false, int backlog = 128, int max_active = 64);
    void listen(const std::string& address, uint16_t port, bool reconnect = false, int backlog = 128, int max_active = 64);
    void accept();
    void close();
    void shutdown_connections();
};

}

#endif // STRTB_NETWORKING_TCP_SERVER_H
