#ifndef STRTB_NETWORKING_TCP_SERVER_H
#define STRTB_NETWORKING_TCP_SERVER_H

#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <string>
#include <set>
#include "../common/deregistration_interface.h"
#include "tcp_server_connection.h"

namespace strtb::networking {

class tcp_server : strtb::common::deregistration_interface<tcp_server_connection*> {
private:
    std::mutex _lock;
    std::condition_variable _connections_cv;
    std::string _server_ip;
    int _ip_family, _server_port, _backlog;
    size_t _max_active, _recv_buffer_size;
    std::set<tcp_server_connection*> _active_connections;

    // Platform-specific
#ifdef __linux__
    int _sock = -1, _event, _af;
    bool _shutdown_sent = true, _shutdown_received = true;
#endif

public:
    tcp_server();
    tcp_server(size_t recv_buffer_size);
    ~tcp_server();
    void listen(const char* address, uint16_t port, bool reuseaddr = true, int backlog = 64, size_t max_active = 64);
    void listen(const std::string& address, uint16_t port, bool reuseaddr = true, int backlog = 64, size_t max_active = 64);
    tcp_server_connection* accept();
    bool shutdown();
    bool close();
    std::string server_ip();
    int server_port();
    int ip_family();
    size_t recv_buffer_size();
    void deregister(tcp_server_connection* target);
};

}

#endif // STRTB_NETWORKING_TCP_SERVER_H
