#include "tcp_server.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/eventfd.h>
#include <poll.h>

using namespace strtb::networking;

struct strtb::networking::tcp_server_platform_specific {
    int sock = -1, event, af;
    bool shutdown_sent = true, shutdown_received = true;
};

tcp_server::tcp_server() : _pl(new tcp_server_platform_specific), _ip_family(0), _port(0), _backlog(0), _max_active(0) {
    // Create new eventfd (used for shutting down server from another thread)
    _pl->event = eventfd(0, 0);
    if (_pl->event == -1)
        throw internal_error("failed to setup internal synchronization mechanism: " + std::string(strerror(errno)), errno);
}

tcp_server::~tcp_server() {
    close();
    ::close(_pl->event);
    delete _pl;
}

void tcp_server::listen(const std::string& address, uint16_t port, bool reconnect, int backlog, int max_active) {
    listen(address.c_str(), port, reconnect, backlog, max_active);
}

void tcp_server::listen(const char* address, uint16_t port, bool reconnect, int backlog, int max_active) {
    std::lock_guard<std::mutex> guard(_lock);

    if (_pl->sock != -1) {
        // Socket is already open
        if (reconnect) {
            ::close(_pl->sock);
            // TODO: Also disconnect all connected clients
        } else {
            throw connection_error("socket already open", 0);
        }
    }

    _pl->shutdown_sent = false;
    _pl->shutdown_received = false;
    _backlog = backlog;
    _max_active = max_active;

    // Convert IP address to appropriate struct
    uint8_t address_struct[sizeof(struct in6_addr)] = {0};
    char address_str_clean[INET6_ADDRSTRLEN] = {0};

    if (inet_pton(AF_INET, address, address_struct) == 1) {             // IPv4 address
        if (inet_ntop(AF_INET, address_struct, address_str_clean, INET6_ADDRSTRLEN) == NULL)
            throw internal_error("internal error while handling IP address: " + std::string(strerror(errno)), errno);
        _ip_family = 4;
        _ip = address_str_clean;
        _port = port;
        _pl->af = AF_INET;
    } else if (inet_pton(AF_INET6, address, address_struct) == 1) {     // IPv6 address
        if (inet_ntop(AF_INET6, address_struct, address_str_clean, INET6_ADDRSTRLEN) == NULL)
            throw internal_error("internal error while handling IP address: " + std::string(strerror(errno)), errno);
        _ip_family = 6;
        _ip = address_str_clean;
        _port = port;
        _pl->af = AF_INET6;
    } else throw address_resolution_error("given address is not a valid IPv4 or IPv6 IP address", 0);

    // Open socket
    _pl->sock = socket(_pl->af, SOCK_STREAM, 0);
    if (_pl->sock == -1) switch (errno) {
    case EACCES:
        _ip = "";
        _ip_family = 0;
        _port = 0;
        throw connection_error(errno);
    default:
        _ip = "";
        _ip_family = 0;
        _port = 0;
        throw internal_error(errno);
    }

    // Bind to socket
    switch (_pl->af) {
    case AF_INET: {
            struct sockaddr_in saddr = {
                .sin_family = AF_INET,
                .sin_port = htons(port),
                .sin_addr = *((struct in_addr*) address_struct),
                .sin_zero = {0}
            };

            if (bind(_pl->sock, (struct sockaddr*) &saddr, sizeof(saddr)) == -1) {
                // Error binding
                connection_error e(errno);
                ::close(_pl->sock);
                _pl->sock = -1;
                _ip = "";
                _ip_family = 0;
                _port = 0;
                throw e;
            }
        }
        break;

    case AF_INET6: {
            struct sockaddr_in6 saddr = {
                .sin6_family = AF_INET6,
                .sin6_port = htons(port),
                .sin6_flowinfo = 0,
                .sin6_addr = *((struct in6_addr*) address_struct),
                .sin6_scope_id = 0
            };

            if (bind(_pl->sock, (struct sockaddr*) &saddr, sizeof(saddr)) == -1) {
                // Error binding
                connection_error e(errno);
                ::close(_pl->sock);
                _pl->sock = -1;
                _ip = "";
                _ip_family = 0;
                _port = 0;
                throw e;
            }
        }
        break;

    default:
        _ip = "";
        _ip_family = 0;
        _port = 0;
        throw internal_error("Unreachable code was reached, this is either a weird bug in Streaming Toolbox, or your CPU is unstable.", 0);
    }

    if (::listen(_pl->sock, backlog) == -1) {
        // Error listening
        connection_error e(errno);
        ::close(_pl->sock);
        _pl->sock = -1;
        _ip = "";
        _ip_family = 0;
        _port = 0;
        throw e;
    }
}

void* tcp_server::accept() {
    if (_pl->sock == -1)
        throw connection_closed("socket closed or hasn't been opened yet", 0);
    if (_pl->shutdown_received)
        return nullptr;

    // Loop until there's a new connection, a shutdown or an error
    while (true) {
        struct pollfd p[] = {
            {
                .fd = _pl->sock,
                .events = POLLIN,
                .revents = 0
            }, {
                .fd = _pl->event,
                .events = POLLIN,
                .revents = 0
            }
        };

        // Block until there's a connection to accept, or until the server shuts down
        if (poll(p, 2, -1) == -1)
            throw internal_error(errno);

        // Check for shutdown event
        if (p[1].revents & (POLLIN | POLLERR)) {
            uint64_t buffer;
            if (read(_pl->event, &buffer, 8) != 8)
                throw internal_error("error in internal synchronization mechanism: " + std::string(strerror(errno)), errno);
            std::lock_guard<std::mutex> guard(_lock);
            _pl->shutdown_received = true;
            return nullptr;
        }

        // Check for incoming connections
        if (p[0].revents & (POLLIN | POLLERR)) {
            // Get incoming connection's address
            char addr[sizeof(struct sockaddr_in6)];
            socklen_t addrlen = sizeof(struct sockaddr_in6);
            int new_sock = ::accept(_pl->sock, (sockaddr*)&addr, &addrlen);

            // Check for errors
            if (new_sock == -1) switch (errno) {
                // Client errors, safe to ignore; caller should just log the error and continue
                case ENETDOWN:
                case EPROTO:
                case ENOPROTOOPT:
                case EHOSTDOWN:
                case ENONET:
                case EHOSTUNREACH:
                case EOPNOTSUPP:
                case ENETUNREACH:
                case ECONNABORTED:
                case EPERM:
                case ETIMEDOUT:
                    throw connection_error(errno);
                // Other more serious errors; caller should abort
                default:
                    throw internal_error(errno);
            }

            char addr_str[INET6_ADDRSTRLEN];
            int port;
            switch (_pl->af) {
            case AF_INET:
                if (addrlen != sizeof(struct sockaddr_in))
                    throw internal_error("unexpected socket address struct length", 0);
                inet_ntop(AF_INET, &((sockaddr_in*) addr)->sin_addr, addr_str, INET_ADDRSTRLEN);
                port = ntohs(((sockaddr_in*) addr)->sin_port);
                break;
            case AF_INET6:
                if (addrlen != sizeof(struct sockaddr_in6))
                    throw internal_error("unexpected socket address struct length", 0);
                inet_ntop(AF_INET6, &((sockaddr_in6*) addr)->sin6_addr, addr_str, INET6_ADDRSTRLEN);
                port = ntohs(((sockaddr_in6*) addr)->sin6_port);
                break;
            default:
                throw internal_error("Unreachable code was reached, this is either a software bug in Streaming Toolbox or one of the plugins, or your system is unstable.", 0);
            }

            printf("Incoming connection from %s:%d\n", addr_str, port);
            return (void*) 1;
        }
    }
}

bool tcp_server::shutdown_incoming() {
    std::lock_guard<std::mutex> guard(_lock);
    uint64_t buf = 1;
    if (_pl->sock == -1)
        return false;
    if (_pl->shutdown_sent)
        return true;
    if (write(_pl->event, &buf, 8) != 8)
        throw internal_error("error in internal synchronization mechanism: " + std::string(strerror(errno)), errno);
    _pl->shutdown_sent = true;
    return true;
}

bool tcp_server::close() {
    std::lock_guard<std::mutex> guard(_lock);
    if (_pl->sock == -1)
        return false;
    ::close(_pl->sock);
    // TODO: Also wait for all connected clients to close
    _ip = "";
    _ip_family = 0;
    _port = 0;
    return true;
}
