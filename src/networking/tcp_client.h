#ifndef STRTB_NETWORKING_TCP_CLIENT_H
#define STRTB_NETWORKING_TCP_CLIENT_H

#include <cstdint>
#include <string>
#include <mutex>
#include "exceptions.h"

#define STRTB_NETWORKING_RECV_BUFFER_SIZE 4096

namespace strtb::networking {

struct tcp_client_platform_specific;

class tcp_client {
protected:
    tcp_client_platform_specific* _pl;
    char _buffer[STRTB_NETWORKING_RECV_BUFFER_SIZE];
    size_t _line_leftovers;
    std::mutex _lock, _send_lock;
public:
    const char* buffer = _buffer;
    tcp_client();
    ~tcp_client();
    void connect(const char* address, uint16_t port, bool reconnect = false);
    void connect(const std::string& address, uint16_t port, bool reconnect = false);
    ssize_t recv();
    ssize_t send(const char* buf, size_t len);
    ssize_t send(const std::string& buf);
    std::lock_guard<std::mutex> acquire_send_lock();
    std::string recv_line(const std::string& endline = "\r\n", size_t max_len = 8192);
    bool shutdown(bool receive = true, bool send = true);
    bool close();
};

}

#endif // STRTB_NETWORKING_TCP_CLIENT_H
