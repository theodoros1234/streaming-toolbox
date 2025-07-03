#ifndef STRTB_NETWORKING_TCP_SOCKET_H
#define STRTB_NETWORKING_TCP_SOCKET_H

#include <string>
#include <mutex>
#include "exceptions.h"     // IWYU pragma: export

#define STRTB_NETWORKING_RECV_BUFFER_SIZE_MIN 256
#define STRTB_NETWORKING_RECV_BUFFER_SIZE_DEFAULT 4096
#define STRTB_NETWORKING_RECV_BUFFER_SIZE_DEFAULT_SSL 16384

namespace strtb::networking {

class tcp_socket {
protected:
    char* _buffer;
    size_t _buffer_size;
    const char padding = 0;   // used just in case some plugin uses old null-terminated C functions on the buffer
    size_t _line_leftovers_pos = 0, _line_leftovers = 0;
    std::recursive_mutex _lock;

    // Platform-specific
#ifdef __linux__
    int _sock = -1;
#endif

public:
    const char* buffer;
    tcp_socket();
    tcp_socket(size_t recv_buffer_size);
    virtual ~tcp_socket();
    ssize_t recv();
    virtual ssize_t recv(size_t max_len);
    virtual ssize_t send(const char* buf, size_t len);
    ssize_t send(const std::string& buf);
    std::string recv_line(const std::string& endline = "\r\n", size_t max_len = 8192);
    void recv_line(std::string& line, const std::string& endline = "\r\n", size_t max_len = 8192);
    void shutdown(bool receive = true, bool send = true);
    virtual void close();
    bool is_open();
    size_t buffer_size();
    void buffer_clear();
};

}

#endif // STRTB_NETWORKING_TCP_SOCKET_H
