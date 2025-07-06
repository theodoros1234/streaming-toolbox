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

class tcp_server : protected strtb::common::deregistration_interface<tcp_server_connection*> {
private:
    // Platform-specific
#ifdef __linux__
    int _sock = -1, _event, _af;
    bool _shutdown_sent = true, _shutdown_received = true;
#endif

protected:
    std::mutex _lock;
    std::condition_variable _connections_cv;
    std::string _server_ip;
    int _ip_family, _server_port, _backlog;
    size_t _max_active, _recv_buffer_size;
    std::set<tcp_server_connection*> _active_connections;
    virtual tcp_server_connection* _new_connection(int sock, std::string remote_ip, int remote_port);

public:
    tcp_server();
    tcp_server(size_t recv_buffer_size);
    virtual ~tcp_server();
    void listen(const char* address, uint16_t port, bool reuseaddr = true, int backlog = 64, size_t max_active = 64);
    void listen(const std::string& address, uint16_t port, bool reuseaddr = true, int backlog = 64, size_t max_active = 64);
    tcp_server_connection* accept();
    bool shutdown();
    bool close();
    const std::string& server_ip() const;
    int server_port() const;
    int ip_family() const;
    size_t recv_buffer_size() const;
    void deregister(tcp_server_connection* target);
#ifdef __linux__
    int fd() const;
#endif
};

}

#endif // STRTB_NETWORKING_TCP_SERVER_H
