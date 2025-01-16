#include "tcp_client.h"
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cstring>

using namespace strtb::networking;

struct strtb::networking::tcp_client_platform_specific {
    int sock = -1;
};

tcp_client::tcp_client() : _pl(new tcp_client_platform_specific), _line_leftovers(0) {}

void tcp_client::connect(const char* address, uint16_t port, bool reconnect) {
    std::lock_guard<std::mutex> guard(_lock);
    if (_pl->sock != -1) {  // Socket is already open (and possibly connected)
        if (reconnect) {
            ::shutdown(_pl->sock, SHUT_RDWR);
            ::close(_pl->sock);
        } else {
            throw connection_closed("socket already open", 0);
        }
    }

    // Get target host info
    struct addrinfo gai_hints = {
        .ai_flags = AI_NUMERICSERV,
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
        .ai_addrlen = 0,
        .ai_addr = NULL,
        .ai_canonname = NULL,
        .ai_next = NULL
    };

    struct addrinfo* gai_result;
    int gai_err;
    switch (gai_err = getaddrinfo(address, std::to_string(port).c_str(), &gai_hints, &gai_result)) {
    case 0: // Success
        break;
    case EAI_ADDRFAMILY:
    case EAI_AGAIN:
    case EAI_FAIL:
    case EAI_NODATA:
    case EAI_NONAME:
        throw address_resolution_error(gai_strerror(gai_err), gai_err);
    default:
        throw internal_error(gai_strerror(gai_err), gai_err);
    }

    int connect_error = 0;
    int connect_error_priority = 0;

    for (struct addrinfo* item = gai_result; item != NULL; item = item->ai_next) {
        // Try to connect to a socket through any of the returned results
        _pl->sock = socket(item->ai_family, item->ai_socktype, item->ai_protocol);
        if (_pl->sock != -1) {  // Socket creation successful
            if (::connect(_pl->sock, item->ai_addr, item->ai_addrlen) == -1) {    // Connection failed
                ::close(_pl->sock);
                _pl->sock = -1;

                // Determine what went wrong
                // Out of all address connections, only the most "important" error will be given back (if all fail).
                switch (errno) {
                case ENETUNREACH:
                    if (connect_error_priority < 1) {
                        connect_error = errno;
                        connect_error_priority = 1;
                    }
                    break;

                case ETIMEDOUT:
                    if (connect_error_priority < 2) {
                        connect_error = errno;
                        connect_error_priority = 2;
                    }
                    break;

                case ECONNREFUSED:
                    if (connect_error_priority < 3) {
                        connect_error = errno;
                        connect_error_priority = 3;
                    }
                    break;

                case EACCES:
                case EPERM:
                    if (connect_error_priority < 4) {
                        connect_error = errno;
                        connect_error_priority = 4;
                    }
                    break;

                default:
                    if (connect_error_priority < 5) {
                        connect_error = errno;
                        connect_error_priority = 5;
                    }
                }
            } else break;       // Connect successful
        } else switch (errno) { // Socket creation failed, determine what went wrong
        case EACCES:
            if (connect_error_priority < 4) {
                connect_error = errno;
                connect_error_priority = 4;
            }
            break;

        default:
            if (connect_error_priority < 5) {
                connect_error = errno;
                connect_error_priority = 5;
            }
        }
    }

    freeaddrinfo(gai_result);

    if (_pl->sock == -1)    // Failed to connect
        throw connection_error(connect_error);
}

void tcp_client::connect(const std::string& address, uint16_t port, bool reconnect) {
    connect(address.c_str(), port, reconnect);
}

tcp_client::~tcp_client() {
    std::lock_guard<std::mutex> guard(_lock);
    if (_pl->sock != -1) {
        ::shutdown(_pl->sock, SHUT_RDWR);
        ::close(_pl->sock);
    }
    delete _pl;
}

ssize_t tcp_client::recv() {
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

ssize_t tcp_client::send(const char* buf, size_t len) {
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

ssize_t tcp_client::send(const std::string& buf) {
    return send(buf.data(), buf.size());
}

std::lock_guard<std::mutex> tcp_client::acquire_send_lock() {
    return std::lock_guard<std::mutex>(_send_lock);
}

static int shutdown_convert[2][2] = {{-1, SHUT_WR}, {SHUT_RD, SHUT_RDWR}};

bool tcp_client::shutdown(bool receive, bool send) {
    if (shutdown_convert[receive][send] == -1) {
        throw std::invalid_argument("nothing to shut down, receive and send arguments are both false");
    }

    std::lock_guard<std::mutex> guard(_lock);
    if (_pl->sock == -1)
        return false;
    ::shutdown(_pl->sock, shutdown_convert[receive][send]);
    return true;
}

bool tcp_client::close() {
    std::lock_guard<std::mutex> guard(_lock);
    if (_pl->sock == -1)
        return false;
    ::close(_pl->sock);
    return true;
}

std::string tcp_client::recv_line(const std::string& endline, size_t max_len) {
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
