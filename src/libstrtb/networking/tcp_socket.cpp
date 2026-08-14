#include "tcp_socket.h"
#include "../logging.h"
#include <cassert>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cstring>
#include <cstdlib>
#include <poll.h>
#include <string>

using namespace strtb;
using namespace strtb::networking;
using namespace std::string_literals;

static logging::source log("TCP Socket", false);

tcp_socket::tcp_socket(bool buffered_send, size_t buffer_size) :
    _buffer_size(buffer_size), _buffered_send(buffered_send) {
    if (buffer_size < STRTB_NETWORKING_RECV_BUFFER_SIZE_MIN)
        throw std::invalid_argument("tcp_socket recv_buffer_size must be at least 256 bytes");
}

tcp_socket::~tcp_socket() {
    if (_sock != -1) {
        log.put(logging::WARNING, {"Destructor called when socket was still open. Closing the socket, but this may lead to a crash. If you're a plugin developer, make sure you call close() on the socket."});
        ::shutdown(_sock, SHUT_RDWR);
        ::close(_sock);
        buffer_clear_recv();
        buffer_clear_send();
    }
    if (_buffer_recv)
        free(_buffer_recv);
    if (_buffer_send)
        free(_buffer_send);
}

void tcp_socket::_prepare_buffers() {
    // allocate buffers if not already allocated

    if (!_buffer_recv) {
        _buffer_recv = (char*) std::malloc(_buffer_size);
        if (!_buffer_recv)
            throw internal_error(errno);
    }

    if (_buffered_send && !_buffer_send) {
        _buffer_send = (char*) std::malloc(_buffer_size);
        if (!_buffer_send)
            throw internal_error(errno);
    }

    buffer_clear_recv();
    buffer_clear_send();
}

void tcp_socket::_movable(tcp_socket &other, const std::type_info &type) {
    if (_sock != -1)
        throw std::logic_error("cannot move socket object while the destination object is already open");
    const std::type_info &other_type = typeid(other);
    if (type != other_type)
        throw std::invalid_argument("cannot move "s + type.name() + " to " + other_type.name());
}

void tcp_socket::_move(tcp_socket &&other) {
    _buffer_recv = std::exchange(other._buffer_recv, nullptr);
    _buffer_send = std::exchange(other._buffer_send, nullptr);
    _buffer_size = other._buffer_size;
    _buffered_send = other._buffered_send;
    _line_leftovers_pos = std::exchange(other._line_leftovers_pos, 0);
    _line_leftovers = std::exchange(other._line_leftovers, 0);
    _send_pos = std::exchange(other._send_pos, 0);
    _sock = std::exchange(other._sock, -1);
}

void tcp_socket::_move_assign(tcp_socket &&other) {
    if (_buffer_recv)
        free(_buffer_recv);
    if (_buffer_send)
        free(_buffer_send);
    _move(std::move(other));
}

tcp_socket::tcp_socket(tcp_socket &&other) {
    // check if types match and move
    _movable(other, typeid(tcp_socket));
    std::lock_guard<std::recursive_mutex> guard(other._lock);
    _move(std::move(other));
}

tcp_socket& tcp_socket::operator=(tcp_socket &&other) {
    // check if types match and move
    _movable(other, typeid(tcp_socket));
    std::lock_guard<std::recursive_mutex> guard(_lock);
    std::lock_guard<std::recursive_mutex> guard_o(other._lock);
    _move_assign(std::move(other));
    return *this;
}

size_t tcp_socket::_recv(size_t len) {
    ssize_t result = ::recv(_sock, _buffer_recv, len, 0);
    if (result == -1) switch (errno) {      // Error
    case ECONNREFUSED:
        throw connection_error(errno);
    default:
        throw internal_error(errno);
    } else return result;                   // Success, or connection closed normally
}

std::pair<const char *, size_t> tcp_socket::recv() {
    return recv(_buffer_size);
}

std::pair<const char *, size_t> tcp_socket::recv(size_t max_len) {
    if (_sock == -1)
        throw connection_closed("socket closed or hasn't been opened yet", 0);

    if (max_len == 0)
        throw std::invalid_argument("max_len cannot be 0");

    if (max_len > _buffer_size)
        max_len = _buffer_size;

    assert(_buffer_recv);

    // Return from recv_line's leftovers if there are any
    if (_line_leftovers) {
        if (_line_leftovers > max_len) {
            // too many leftovers, just take a piece from them
            std::pair<const char*, size_t> r = {_buffer_recv + _line_leftovers_pos, max_len};
            _line_leftovers_pos += max_len;
            _line_leftovers -= max_len;
            return r;
        } else {
            // take the entire leftovers and reset
            std::pair<const char*, size_t> r = {_buffer_recv + _line_leftovers_pos, _line_leftovers};
            _line_leftovers_pos = 0;
            _line_leftovers = 0;
            return r;
        }
    }

    // No leftovers, get new data from network
    return std::make_pair(_buffer_recv, _recv(max_len));
}

void tcp_socket::_send(const char* buf, size_t len) {
    while (len > 0) {
        ssize_t result = ::send(_sock, buf, len, MSG_NOSIGNAL);
        if (result == -1) {     // error
            switch (errno) {
            case ECONNRESET:
                throw connection_error(errno);
            case EPIPE:
            case ENOTCONN:
                throw connection_closed(errno);
            default:
                throw internal_error(errno);
            }
        } else {        // maybe partial send
            len -= result;
            buf += result;
        }
    }
}

void tcp_socket::send(const char* buf, size_t len) {
    if (_sock == -1)
        throw connection_closed("socket closed or hasn't been opened yet", 0);

    if (_buffered_send) {
        assert(_buffer_send);

        while (len > 0) {
            if (len >= _buffer_size && _send_pos == 0) {
                // empty buffer and input is bigger than it => send directly without storing in buffer
                _send(buf, _buffer_size);
                len -= _buffer_size;
                buf += _buffer_size;
            } else {
                // store into send buffer without it overflowing
                size_t len_to_store = std::min(len, _buffer_size - _send_pos);
                std::memcpy(_buffer_send + _send_pos, buf, len_to_store);
                _send_pos += len_to_store;
                len -= len_to_store;
                buf += len_to_store;

                // flush if send buffer is full
                if (_send_pos == _buffer_size)
                    flush();
            }
        }
    } else {
        _send(buf, len);
    }
}

void tcp_socket::send(const std::string& buf) {
    return send(buf.data(), buf.size());
}

void tcp_socket::flush() {
    if (_sock == -1)
        throw connection_closed("socket closed or hasn't been opened yet", 0);
    if (!_buffered_send)
        throw connection_error("buffered send is not enabled", 0);
    if (_send_pos == 0)
        return;

    assert(_buffer_send);
    _send(_buffer_send, _send_pos);
    _send_pos = 0;
}

static int shutdown_convert[2][2] = {{-1, SHUT_WR}, {SHUT_RD, SHUT_RDWR}};

void tcp_socket::shutdown(bool receive, bool send) {
    if (shutdown_convert[receive][send] == -1)
        throw std::invalid_argument("nothing to shut down, receive and send arguments are both false");

    std::lock_guard<std::recursive_mutex> guard(_lock);
    if (_sock != -1)
        ::shutdown(_sock, shutdown_convert[receive][send]);
}

void tcp_socket::close() {
    std::lock_guard<std::recursive_mutex> guard(_lock);
    if (_sock == -1)
        throw connection_closed("socket already closed or never opened", 0);
    shutdown(true, true);
    int close_ret = ::close(_sock);
    _sock = -1;
    _send_pos = 0;
    _line_leftovers = 0;
    if (close_ret)
        throw internal_error(errno);
}

bool tcp_socket::is_open() const {
    return _sock != -1;
}

std::pair<std::string, bool> tcp_socket::recv_line(bool strip_endline, const std::string& endline, size_t max_len) {
    std::string line;
    return std::make_pair(line, recv_line(line, strip_endline, endline, max_len));
}

// WARNING: CRLF won't be detected when hitting the length limit in the middle of it
bool tcp_socket::recv_line(std::string& line, bool strip_endline, const std::string& endline, size_t max_len) {
    line.clear();
    if (_sock == -1)
        throw connection_closed("socket closed or hasn't been opened yet", 0);
    // Up to 2 characters allowed for endline argument
    if (endline.size() > 2)
        throw std::invalid_argument("recv_line: endline must have at most 2 characters");
    // endline cannot be empty
    if (endline.empty())
        throw std::invalid_argument("recv_line: endline cannot be empty");
    // max_len must at least be as long as the endline
    if (max_len < endline.size())
        throw std::invalid_argument("recv_line: max_len must be at least as long as the endline");

    assert(_buffer_recv);
    size_t received = 0, i = 0;
    const char* buf = nullptr;
    bool more = true, endline_reached = false;
    do {
        std::tie(buf, received) = recv();

        // Stop if no data was received (socket is closed)
        if (received == 0)
            return false;
        // Examine each block of received data separately
        for (i=0; i<received; i++) {
            line.push_back(buf[i]);

            // Check for endline
            if (buf[i] == endline.back()) {
                // Also handle 2 character endlines
                if (endline.size() == 2) {
                    if (line.size() >= 2 && line[line.size() - 2] == endline.front()) {
                        more = false;
                        endline_reached = true;
                        if (strip_endline)
                            line.resize(line.length() - 2);
                        break;
                    }
                } else {
                    more = false;
                    endline_reached = true;
                    if (strip_endline)
                        line.pop_back();
                    break;
                }
            }

            // Check for maximum length
            if (line.size() >= max_len) {
                more = false;
                break;
            }
        }
    } while (more);
    i++;    // fixes following math

    // Mark any excess bytes left in the receive buffer
    if (i < received) {
        _line_leftovers = received - i;
        _line_leftovers_pos = i + (buf - _buffer_recv);
    } else {
        _line_leftovers = 0;
    }

    return endline_reached;
}

size_t tcp_socket::buffer_size() const {
    return _buffer_size;
}

void tcp_socket::buffer_clear_recv() {
    std::memset(_buffer_recv, 0, _buffer_size);
}

void tcp_socket::buffer_clear_send() {
    if (_buffer_send)
        std::memset(_buffer_send, 0, _buffer_size);
}

bool tcp_socket::buffered_send() const {
    return _buffer_send != nullptr;
}

#ifdef __linux__
int tcp_socket::fd() const {return _sock;}
#endif

bool tcp_socket::available() {
    if (_sock == -1)
        throw connection_closed("socket closed or hasn't been opened yet", 0);
    assert(_buffer_recv);

    if (_line_leftovers)    // leftovers immediately available
        return true;
    else    // check for immediately available data (or shutdown/error) from the socket
        return _available();

    /* Note that some error conditions might immediately raise an exception (particularly SSL),
     * because it is unsafe to keep using the socket afterwards. Others (cleartext sockets)
     * may defer the error to the next recv call. Callers must be ready to handle both cases.
     */
}

bool tcp_socket::_available() {
    // check if there's any data immediately available to read (or shutdown/error)
    struct pollfd p = {_sock, POLLIN, 0};
    int ret = poll(&p, 1, 0);

    if (ret < -1)
        throw internal_error(errno);
    else
        return ret;
}
