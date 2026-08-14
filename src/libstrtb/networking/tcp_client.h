#ifndef STRTB_NETWORKING_TCP_CLIENT_H
#define STRTB_NETWORKING_TCP_CLIENT_H

#include <time.h>

#include "tcp_socket.h"
#include "../shutdown_controller.h"

namespace strtb::networking {

class tcp_client : public tcp_socket, public shutdown_controllable {
protected:
    std::string _remote_ip;
    int _remote_port = 0;
    bool _connecting = false, _cancel_sent = false,
         _connect_restrict = false, _shutdown_controller_state = false;
    shutdown_controller *_shutdown_controller = nullptr;

    // NOTE: cancel_connect/shutdown could be missed when moving, use a shutdown_controller if that's a problem
    void shutdown_controllable_signal(bool state);
    void _movable(tcp_client &other, const std::type_info &type);
    void _move(tcp_client &&other);
    void _move_assign(tcp_client &&other);

    // Platform-specific
#ifdef __linux__
    int _event = -1;
#endif

public:
    tcp_client(bool buffered_send = false, size_t buffer_size = STRTB_NETWORKING_RECV_BUFFER_SIZE_DEFAULT);
    tcp_client(tcp_client &&other);
    ~tcp_client();
    tcp_client& operator=(tcp_client &&other);
    void connect(const char* address, uint16_t port, time_t timeout = 30);
    void connect(const std::string& address, uint16_t port, time_t timeout = 30);
    void cancel_connect();
    void reset();
    void close();
    const std::string& remote_ip() const;
    int remote_port() const;
    void attach_shutdown_controller(shutdown_controller &ctrl);
    void detach_shutdown_controller();
    void release();
};

}

#endif // STRTB_NETWORKING_TCP_CLIENT_H
