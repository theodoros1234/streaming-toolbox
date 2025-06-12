#ifndef STRTB_NETWORKING_TCP_SOCKET_H
#define STRTB_NETWORKING_TCP_SOCKET_H

#include <string>
#include <mutex>
#include "exceptions.h"     // IWYU pragma: export

#define STRTB_NETWORKING_RECV_BUFFER_SIZE 4096

namespace strtb::networking {

class tcp_socket {
protected:
    char _buffer[STRTB_NETWORKING_RECV_BUFFER_SIZE];
    size_t _line_leftovers = 0;
    std::recursive_mutex _lock;

    // Platform-specific
#ifdef __linux__
    int _sock = -1;
#endif

public:
    const char* buffer = _buffer;
    tcp_socket();
    virtual ~tcp_socket();
    ssize_t recv();
    ssize_t send(const char* buf, size_t len);
    ssize_t send(const std::string& buf);
    std::string recv_line(const std::string& endline = "\r\n", size_t max_len = 8192);
    void shutdown(bool receive = true, bool send = true);
    bool close();
    bool is_open();
};

}

#endif // STRTB_NETWORKING_TCP_SOCKET_H
