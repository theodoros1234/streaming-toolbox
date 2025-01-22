#ifndef STRTB_NETWORKING_TCP_SERVER_H
#define STRTB_NETWORKING_TCP_SERVER_H

#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <string>
#include <set>
#include <queue>
#include "../common/deregistration_interface.h"
#include "exceptions.h"
#include "tcp_server_connection.h"

namespace strtb::networking {

struct tcp_server_platform_specific;

class tcp_server : strtb::common::deregistration_interface<tcp_server_connection*> {
private:
    tcp_server_platform_specific* _pl;
    std::mutex _lock;
    std::condition_variable _connections_cv;
    std::string _server_ip;
    int _ip_family, _server_port, _backlog;
    size_t _max_active;
    std::set<tcp_server_connection*> _active_connections;

public:
    tcp_server();
    ~tcp_server();
    void listen(const char* address, uint16_t port, bool reconnect = false, int backlog = 128, size_t max_active = 64);
    void listen(const std::string& address, uint16_t port, bool reconnect = false, int backlog = 128, size_t max_active = 64);
    tcp_server_connection* accept();
    bool shutdown();
    bool close();
    std::string server_ip();
    int server_port();
    int ip_family();
    void deregister(tcp_server_connection* target);
};

}

#endif // STRTB_NETWORKING_TCP_SERVER_H
