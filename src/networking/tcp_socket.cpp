#include "tcp_socket.h"
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cstring>

using namespace strtb::networking;

tcp_socket::tcp_socket() : _pl(new tcp_socket::platform_specific) {}

tcp_socket::~tcp_socket() {
    std::lock_guard<std::mutex> guard(_lock);
    if (_pl->sock != -1) {
        ::shutdown(_pl->sock, SHUT_RDWR);
        ::close(_pl->sock);
    }
    delete _pl;
}

ssize_t tcp_socket::recv() {
    ssize_t result = ::recv(_pl->sock, _buffer + _line_leftovers, STRTB_NETWORKING_RECV_BUFFER_SIZE - _line_leftovers, 0);
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
    int sock;
    {
        // Check that the socket has been opened before sending through it.
        // However, it is still NOT safe to close an open socket when there are still other threads
        // that could send to it. If you need to close an open socket for reconnecting,
        // while other threads might want to send data, call connect(..., reconnect=true).
        // NOTE: Closing is different than shutting down, shutdown is safe.
        std::lock_guard<std::mutex> guard(_lock);
        sock = _pl->sock;
        if (sock == -1)
            throw connection_closed("socket closed or hasn't been opened yet", 0);
    }

    ssize_t result = ::send(sock, buf, len, MSG_NOSIGNAL);
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

std::lock_guard<std::mutex> tcp_socket::acquire_send_lock() {
    return std::lock_guard<std::mutex>(_send_lock);
}

static int shutdown_convert[2][2] = {{-1, SHUT_WR}, {SHUT_RD, SHUT_RDWR}};

bool tcp_socket::shutdown(bool receive, bool send) {
    if (shutdown_convert[receive][send] == -1) {
        throw std::invalid_argument("nothing to shut down, receive and send arguments are both false");
    }

    std::lock_guard<std::mutex> guard(_lock);
    if (_pl->sock == -1)
        return false;
    ::shutdown(_pl->sock, shutdown_convert[receive][send]);
    return true;
}

bool tcp_socket::close() {
    std::lock_guard<std::mutex> guard(_lock);
    if (_pl->sock == -1)
        return false;
    ::close(_pl->sock);
    return true;
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
