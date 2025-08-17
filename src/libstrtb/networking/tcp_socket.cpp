#include "tcp_socket.h"
#include "../logging/logging.h"
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cstring>
#include <cstdlib>

using namespace strtb;
using namespace strtb::networking;

static logging::source log("TCP Socket", false);

tcp_socket::tcp_socket() : tcp_socket(STRTB_NETWORKING_RECV_BUFFER_SIZE_DEFAULT) {}

tcp_socket::tcp_socket(size_t recv_buffer_size) : _buffer_size(recv_buffer_size) {
    if (recv_buffer_size < STRTB_NETWORKING_RECV_BUFFER_SIZE_MIN)
        throw std::invalid_argument("tcp_socket recv_buffer_size must be at least 256 bytes");
    _buffer = (char*) std::malloc(recv_buffer_size);
    if (!_buffer)
        throw internal_error("Failed to allocate memory for TCP socket recv buffer: " + std::string(std::strerror(errno)), errno);
    buffer = _buffer;
    buffer_clear();
}

tcp_socket::~tcp_socket() {
    if (_sock != -1) {
        log.put(logging::WARNING, {"Destructor called when socket was still open. Closing the socket, but this may lead to a crash. If you're a plugin developer, make sure you call close() on the socket."});
        ::shutdown(_sock, SHUT_RDWR);
        ::close(_sock);
        buffer_clear();
    }
    free(_buffer);
}

ssize_t tcp_socket::recv() {
    return recv(_buffer_size);
}

ssize_t tcp_socket::recv(size_t max_len) {
    if (_sock == -1)
        throw connection_closed("socket closed or hasn't been opened yet", 0);

    if (max_len == 0)
        throw std::invalid_argument("max_len cannot be 0");

    if (max_len > _buffer_size)
        max_len = _buffer_size;

    // Return from recv_line's leftovers if there are enough to cover the request
    if (max_len <= _line_leftovers) {   // TODO: I haven't properly tested this part, but it should be working
        memmove(_buffer, _buffer + _line_leftovers_pos, max_len);
        _line_leftovers_pos += max_len;
        _line_leftovers -= max_len;
        return max_len;
    }

    // Move back any leftover bytes from recv_line
    if (_line_leftovers) {
        memmove(_buffer, _buffer + _line_leftovers_pos, _line_leftovers);
        _line_leftovers = 0;
    }

    ssize_t result = ::recv(_sock, _buffer + _line_leftovers, max_len - _line_leftovers, 0);
    if (result == -1) switch (errno) {      // Error
    case ECONNREFUSED:
        throw connection_error(errno);
    default:
        throw internal_error(errno);
    } else {                                // Success, or connection closed normally
        ssize_t r = result + _line_leftovers;
        _line_leftovers = 0;
        return r;
    }
}

ssize_t tcp_socket::send(const char* buf, size_t len) {
    if (_sock == -1)
        throw connection_closed("socket closed or hasn't been opened yet", 0);

    ssize_t result = ::send(_sock, buf, len, MSG_NOSIGNAL);
    if (result == -1) switch (errno) {      // Error
    case ECONNRESET:
        throw connection_error(errno);
    case EPIPE:
    case ENOTCONN:
        throw connection_closed(errno);
    default:
        throw internal_error(errno);
    } else return result;
}

ssize_t tcp_socket::send(const std::string& buf) {
    return send(buf.data(), buf.size());
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
    buffer_clear();
    if (close_ret)
        throw internal_error(errno);
}

bool tcp_socket::is_open() const {
    return _sock != -1;
}

std::string tcp_socket::recv_line(const std::string& endline, size_t max_len) {
    std::string line;
    recv_line(line, endline, max_len);
    return line;
}

void tcp_socket::recv_line(std::string& line, const std::string& endline, size_t max_len) {
    line.clear();
    // Up to 2 characters allowed for endline argument
    if (endline.size() > 2)
        throw std::invalid_argument("recv_line: endline must have at most 2 characters");
    // endline cannot be empty
    if (endline.empty())
        throw std::invalid_argument("recv_line: endline cannot be empty");
    // max_len must at least be as long as the endline
    if (max_len < endline.size())
        throw std::invalid_argument("recv_line: max_len must be at least as long as the endline");

    ssize_t received, i;
    bool more = true;
    do {
        if (_line_leftovers) {
            // Read any previous leftovers first
            received = recv(_line_leftovers);
        } else {
            // Keep receiving more data while the line has not ended yet
            received = recv();
        }
        // Stop if no data was received (socket is closed)
        if (received == 0)
            return;
        // Examine each block of received data separately
        for (i=0; i<received; i++) {
            line.push_back(_buffer[i]);

            // Check for maximum length
            if (line.size() >= max_len) {
                more = false;
                break;
            }

            // Check for endline
            if (_buffer[i] == endline.back()) {
                // Also handle 2 character endlines
                if (endline.size() == 2) {
                    if (line.size() >= 2 && line[line.size() - 2] == endline.front()) {
                        more = false;
                        break;
                    }
                } else {
                    more = false;
                    break;
                }
            }
        }
    } while (more);
    i++;    // fixes following math

    // Mark any excess bytes left in the receive buffer
    if (i < received) {
        _line_leftovers = received - i;
        _line_leftovers_pos = i;
    } else {
        _line_leftovers = 0;
    }
}

size_t tcp_socket::buffer_size() const {
    return _buffer_size;
}

void tcp_socket::buffer_clear() {
    std::memset(_buffer, 0, _buffer_size);
}

#ifdef __linux__
int tcp_socket::fd() const {return _sock;}
#endif
