#ifndef STRTB_NETWORKING_TCP_SOCKET_H
#define STRTB_NETWORKING_TCP_SOCKET_H

#include <cstdint>
#include <string>
#include <mutex>
#include "exceptions.h"

#define STRTB_NETWORKING_RECV_BUFFER_SIZE 4096

namespace strtb::networking {

class tcp_socket {
protected:
    struct platform_specific {
        int sock = -1;
    };

    struct platform_specific* _pl;  // Using a pointer, so platform differences don't change sizeof(_pl), which would break ABI compatibility
    char _buffer[STRTB_NETWORKING_RECV_BUFFER_SIZE];
    size_t _line_leftovers = 0;
    std::mutex _lock, _send_lock;

public:
    const char* buffer = _buffer;
    tcp_socket();
    virtual ~tcp_socket();
    ssize_t recv();
    ssize_t send(const char* buf, size_t len);
    ssize_t send(const std::string& buf);
    std::lock_guard<std::mutex> acquire_send_lock();
    std::string recv_line(const std::string& endline = "\r\n", size_t max_len = 8192);
    bool shutdown(bool receive = true, bool send = true);
    bool close();
    bool is_open();
};

}

#endif // STRTB_NETWORKING_TCP_SOCKET_H
