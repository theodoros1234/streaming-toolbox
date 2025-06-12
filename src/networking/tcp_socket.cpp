#include "tcp_socket.h"
#include "../logging/logging.h"
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cstring>

using namespace strtb;
using namespace strtb::networking;

static logging::source log("TCP Socket");

tcp_socket::tcp_socket() {}

tcp_socket::~tcp_socket() {
    if (_sock != -1) {
        log.put(logging::WARNING, {"Destructor called when socket was still open. Closing the socket, but this may lead to a crash. If you're a plugin developer, make sure you call close() on the socket after all threads that use it have been stopped."});
        ::shutdown(_sock, SHUT_RDWR);
        ::close(_sock);
    }
}

ssize_t tcp_socket::recv() {
    if (_sock == -1)
        throw connection_closed("socket closed or hasn't been opened yet", 0);

    ssize_t result = ::recv(_sock, _buffer + _line_leftovers, STRTB_NETWORKING_RECV_BUFFER_SIZE - _line_leftovers, 0);
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
    if (shutdown_convert[receive][send] == -1) {
        throw std::invalid_argument("nothing to shut down, receive and send arguments are both false");
    }

    std::lock_guard<std::recursive_mutex> guard(_lock);
    if (_sock != -1)
        ::shutdown(_sock, shutdown_convert[receive][send]);
}

bool tcp_socket::close() {
    if (_sock == -1)
        return false;
    ::close(_sock);
    _sock = -1;
    return true;
}

bool tcp_socket::is_open() {
    std::lock_guard<std::recursive_mutex> guard(_lock);
    return _sock != -1;
}

std::string tcp_socket::recv_line(const std::string& endline, size_t max_len) {
    // Up to 2 characters allowed for endline argument
    if (endline.size() > 2)
        throw std::invalid_argument("recv_line: endline must have at most 2 characters");
    // endline cannot be empty
    if (endline.empty())
        throw std::invalid_argument("recv_line: endline cannot be empty");
    // max_len must at least be as long as the endline
    if (max_len < endline.size())
        throw std::invalid_argument("recv_line: max_len must be at least as long as the endline");

    std::string line;
    ssize_t received, i;
    bool more = true;
    do {
        if (_line_leftovers) {
            // Read any previous leftovers first
            received = _line_leftovers;
            _line_leftovers = 0;
        } else {
            // Keep receiving more data while the line has not ended yet
            received = recv();
        }
        // Stop if no data was received (socket is closed)
        if (received == 0)
            return line;
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

    // Move back any excess bytes left in the receive buffer
    if (i < received) {
        _line_leftovers = received - i;
        memmove(_buffer, _buffer + i, _line_leftovers);
    } else {
        _line_leftovers = 0;
    }
    return line;
}
