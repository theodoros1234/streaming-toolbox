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
#include "../shutdown_controller.h"

namespace strtb::networking {

class tcp_server : protected strtb::common::deregistration_interface<tcp_server_connection*>,
                   public shutdown_controllable {
private:
    shutdown_controller *_shutdown_controller = nullptr;
    bool _shutdown_controller_state = false;

    bool _shutdown();

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
    size_t _max_active, _buffer_size;
    bool _buffered_send;

    // Platform-specific
#ifdef __linux__
    std::vector<bound_port> _socks;
    int _event;
    bool _shutdown_sent = true, _shutdown_received = true;
#endif

    std::set<tcp_server_connection*> _active_connections;
    virtual tcp_server_connection* _new_connection(const bound_port& server, int sock, std::string remote_ip, int remote_port);
    void shutdown_controllable_signal(bool state);

public:
    tcp_server(bool buffered_send = false, size_t buffer_size = STRTB_NETWORKING_RECV_BUFFER_SIZE_DEFAULT);
    tcp_server(const tcp_server&) = delete;
    tcp_server(tcp_server&&) = delete;
    virtual ~tcp_server();
    size_t max_active();
    void max_active(size_t value);
    void listen(const char* address, uint16_t port, bool reuseaddr = true, int backlog = 64);
    void listen(const std::string& address, uint16_t port, bool reuseaddr = true, int backlog = 64);
    tcp_server_connection* accept();
    bool shutdown();
    bool close(bool pre_accept = false);
    std::vector<bound_port> bound_ports();
    size_t buffer_size() const;
    bool buffered_send() const;
    void deregister(tcp_server_connection* target);
    void attach_shutdown_controller(shutdown_controller &ctrl);
    void detach_shutdown_controller();
};

}

#endif // STRTB_NETWORKING_TCP_SERVER_H
