#ifndef STRTB_NETWORKING_TCP_SERVER_H
#define STRTB_NETWORKING_TCP_SERVER_H

#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <string>
#include <set>
#include <vector>
#include "../common/deregistration_interface.h"
#include "tcp_server_connection.h"

namespace strtb::networking {

class tcp_server : protected strtb::common::deregistration_interface<tcp_server_connection*> {
public:
#ifdef __linux__
    struct bound_port {
        int sock, af;
        std::string server_ip;
        int ip_family, server_port, backlog;
    };
#endif

protected:
    std::mutex _lock;
    std::condition_variable _connections_cv;
    size_t _max_active, _recv_buffer_size;

    // Platform-specific
#ifdef __linux__
    std::vector<bound_port> _socks;
    int _event;
    bool _shutdown_sent = true, _shutdown_received = true;
#endif

    std::set<tcp_server_connection*> _active_connections;
    virtual tcp_server_connection* _new_connection(const bound_port& server, int sock, std::string remote_ip, int remote_port);

public:
    tcp_server();
    tcp_server(size_t recv_buffer_size);
    virtual ~tcp_server();
    size_t max_active();
    void max_active(size_t value);
    void listen(const char* address, uint16_t port, bool reuseaddr = true, int backlog = 64);
    void listen(const std::string& address, uint16_t port, bool reuseaddr = true, int backlog = 64);
    tcp_server_connection* accept();
    bool shutdown();
    bool close(bool pre_accept = false);
    std::vector<bound_port> bound_ports();
    size_t recv_buffer_size() const;
    void deregister(tcp_server_connection* target);
};

}

#endif // STRTB_NETWORKING_TCP_SERVER_H
