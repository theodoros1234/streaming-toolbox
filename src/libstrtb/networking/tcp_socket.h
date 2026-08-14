#ifndef STRTB_NETWORKING_TCP_SOCKET_H
#define STRTB_NETWORKING_TCP_SOCKET_H

#include <string>
#include <mutex>
#include <utility>
#include <typeinfo>
#include "exceptions.h"     // IWYU pragma: export

#define STRTB_NETWORKING_RECV_BUFFER_SIZE_MIN 256
#define STRTB_NETWORKING_RECV_BUFFER_SIZE_DEFAULT 4096
#define STRTB_NETWORKING_RECV_BUFFER_SIZE_DEFAULT_SSL 16384
#define STRTB_NETWORKING_RECV_LINE_MAX_LEN_DEFAULT 8192

namespace strtb::networking {

class tcp_socket {
protected:
    char *_buffer_recv = nullptr, *_buffer_send = nullptr;
    size_t _buffer_size = 0;
    bool _buffered_send = false;
    size_t _line_leftovers_pos = 0, _line_leftovers = 0, _send_pos = 0;
    std::recursive_mutex _lock;

    // Platform-specific
#ifdef __linux__
    int _sock = -1;
#endif

    void _prepare_buffers();

    virtual size_t _recv(size_t len);
    virtual void _send(const char* buf, size_t len);
    virtual bool _available();

    void _movable(tcp_socket &other, const std::type_info &type);
    void _move(tcp_socket &&other);
    void _move_assign(tcp_socket &&other);

public:
    tcp_socket(bool buffered_send = false, size_t buffer_size = STRTB_NETWORKING_RECV_BUFFER_SIZE_DEFAULT);
    tcp_socket(const tcp_socket&) = delete;
    tcp_socket(tcp_socket &&other);
    tcp_socket& operator=(const tcp_socket&) = delete;
    tcp_socket& operator=(tcp_socket &&other);
    virtual ~tcp_socket();
    std::pair<const char*, size_t> recv();
    std::pair<const char*, size_t> recv(size_t max_len);
    void send(const char* buf, size_t len);
    void send(const std::string& buf);
    void flush();
    std::pair<std::string, bool> recv_line(bool strip_endline = false, const std::string& endline = "\r\n",
                                           size_t max_len = STRTB_NETWORKING_RECV_LINE_MAX_LEN_DEFAULT);
    bool recv_line(std::string& line, bool strip_endline = false, const std::string& endline = "\r\n",
                   size_t max_len = STRTB_NETWORKING_RECV_LINE_MAX_LEN_DEFAULT);
    void shutdown(bool receive = true, bool send = true);
    virtual void close();
    bool is_open() const;
    bool buffered_send() const;
    size_t buffer_size() const;
    void buffer_clear_recv();
    void buffer_clear_send();
    bool available();
#ifdef __linux__
    int fd() const;
#endif
};

}

#endif // STRTB_NETWORKING_TCP_SOCKET_H
