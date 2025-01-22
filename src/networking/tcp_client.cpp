#include "tcp_client.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cstring>

using namespace strtb::networking;

tcp_client::tcp_client() : tcp_socket() {}

void tcp_client::connect(const char* address, uint16_t port, bool reconnect) {
    std::lock_guard<std::mutex> guard(_lock);
    if (_pl->sock != -1) {  // Socket is already open (and possibly connected)
        if (reconnect) {
            ::shutdown(_pl->sock, SHUT_RDWR);
            ::close(_pl->sock);
        } else {
            throw connection_error("socket already open", 0);
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

    struct addrinfo* item = NULL;
    for (item = gai_result; item != NULL; item = item->ai_next) {
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

    // Store remote host's IP and port
    if (_pl->sock != -1) {
        char remote_ip_char[INET6_ADDRSTRLEN] = {0};
        switch (item->ai_family) {
        case AF_INET:
            if (inet_ntop(AF_INET, &((sockaddr_in*) item->ai_addr)->sin_addr, remote_ip_char, INET6_ADDRSTRLEN) == NULL)
                _remote_ip = "unknown";
            else
                _remote_ip = remote_ip_char;
            _remote_port = ntohs(((sockaddr_in*) item->ai_addr)->sin_port);
            break;

        case AF_INET6:
            if (inet_ntop(AF_INET6, &((sockaddr_in6*) item->ai_addr)->sin6_addr, remote_ip_char, INET6_ADDRSTRLEN) == NULL)
                _remote_ip = "unknown";
            else
                _remote_ip = remote_ip_char;
            _remote_port = ntohs(((sockaddr_in6*) item->ai_addr)->sin6_port);
            break;

        default:
            _remote_ip = "unknown";
        }
    }

    freeaddrinfo(gai_result);

    if (_pl->sock == -1)    // Failed to connect
        throw connection_error(connect_error);
}

void tcp_client::connect(const std::string& address, uint16_t port, bool reconnect) {
    connect(address.c_str(), port, reconnect);
}

bool tcp_client::close() {
    _remote_ip = "";
    _remote_port = 0;
    return tcp_socket::close();
}

std::string tcp_client::remote_ip() {
    std::lock_guard<std::mutex> guard(_lock);
    return _remote_ip;
}

int tcp_client::remote_port() {
    std::lock_guard<std::mutex> guard(_lock);
    return _remote_port;
}
