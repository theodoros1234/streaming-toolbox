#include "tcp_server.h"
#include "../logging/logging.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

// Platform-specific
#ifdef __linux__
#include <sys/eventfd.h>
#include <poll.h>
#endif

using namespace strtb::networking;
using namespace strtb;

static logging::source log("TCP Server");

tcp_server::tcp_server() : tcp_server(STRTB_NETWORKING_RECV_BUFFER_SIZE_DEFAULT) {}

tcp_server::tcp_server(size_t recv_buffer_size) : _ip_family(0), _server_port(0), _backlog(0), _max_active(0) {
    _recv_buffer_size = recv_buffer_size;
    // Create new eventfd (used for shutting down server from another thread)
    _event = eventfd(0, 0);
    if (_event == -1)
        throw internal_error("failed to setup internal synchronization mechanism: " + std::string(strerror(errno)), errno);
}

tcp_server::~tcp_server() {
    if (_sock != -1 || !_active_connections.empty()) {
        log.put(logging::WARNING, {"Destructor called when server on ", _server_ip, ":", _server_port, " was still open. Closing the server, but this may lead to a crash. If you're a plugin developer, make sure you call close() on the server."});
        close();
    }
    ::close(_event);
}

void tcp_server::listen(const std::string& address, uint16_t port, bool reuseaddr, int backlog, size_t max_active) {
    listen(address.c_str(), port, reuseaddr, backlog, max_active);
}

void tcp_server::listen(const char* address, uint16_t port, bool reuseaddr, int backlog, size_t max_active) {
    std::lock_guard<std::mutex> guard(_lock);

    if (_sock != -1)
        throw connection_error("socket already open", 0);

    _shutdown_sent = false;
    _shutdown_received = false;
    _backlog = backlog;
    _max_active = max_active;

    // Convert IP address to appropriate struct
    uint8_t address_struct[sizeof(struct in6_addr)] = {0};
    char address_str_clean[INET6_ADDRSTRLEN] = {0};

    if (inet_pton(AF_INET, address, address_struct) == 1) {             // IPv4 address
        if (inet_ntop(AF_INET, address_struct, address_str_clean, INET6_ADDRSTRLEN) == NULL)
            throw internal_error("internal error while handling IP address: " + std::string(strerror(errno)), errno);
        _ip_family = 4;
        _server_ip = address_str_clean;
        _server_port = port;
        _af = AF_INET;
    } else if (inet_pton(AF_INET6, address, address_struct) == 1) {     // IPv6 address
        if (inet_ntop(AF_INET6, address_struct, address_str_clean, INET6_ADDRSTRLEN) == NULL)
            throw internal_error("internal error while handling IP address: " + std::string(strerror(errno)), errno);
        _ip_family = 6;
        _server_ip = address_str_clean;
        _server_port = port;
        _af = AF_INET6;
    } else throw address_resolution_error("given address is not a valid IPv4 or IPv6 IP address", 0);

    // Open socket
    _sock = socket(_af, SOCK_STREAM, 0);
    if (_sock == -1) switch (errno) {
    case EACCES:
        _server_ip = "";
        _ip_family = 0;
        _server_port = 0;
        throw connection_error(errno);
    default:
        _server_ip = "";
        _ip_family = 0;
        _server_port = 0;
        throw internal_error(errno);
    }

    // Set reuseaddr if needed
    if (reuseaddr) {
        int value = 1;
        if (setsockopt(_sock, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)))
            throw internal_error(errno);
    }

    // Bind to socket
    switch (_af) {
    case AF_INET: {
            struct sockaddr_in saddr = {
                .sin_family = AF_INET,
                .sin_port = htons(port),
                .sin_addr = *((struct in_addr*) address_struct),
                .sin_zero = {0}
            };

            if (bind(_sock, (struct sockaddr*) &saddr, sizeof(saddr)) == -1) {
                // Error binding
                connection_error e(errno);
                ::close(_sock);
                _sock = -1;
                _server_ip = "";
                _ip_family = 0;
                _server_port = 0;
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

            if (bind(_sock, (struct sockaddr*) &saddr, sizeof(saddr)) == -1) {
                // Error binding
                connection_error e(errno);
                ::close(_sock);
                _sock = -1;
                _server_ip = "";
                _ip_family = 0;
                _server_port = 0;
                throw e;
            }
        }
        break;

    default:
        _server_ip = "";
        _ip_family = 0;
        _server_port = 0;
        throw internal_error("Unreachable code was reached, this is either a weird bug in Streaming Toolbox, or your CPU is unstable.", 0);
    }

    if (::listen(_sock, backlog) == -1) {
        // Error listening
        connection_error e(errno);
        ::close(_sock);
        _sock = -1;
        _server_ip = "";
        _ip_family = 0;
        _server_port = 0;
        throw e;
    }
}

tcp_server_connection* tcp_server::accept() {
    if (_sock == -1)
        throw connection_closed("socket closed or hasn't been opened yet", 0);
    if (_shutdown_received)
        return nullptr;

    // Loop until there's a new connection, a shutdown or an error
    while (true) {
        struct pollfd p[] = {
            {
                .fd = _sock,
                .events = POLLIN,
                .revents = 0
            }, {
                .fd = _event,
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
            if (read(_event, &buffer, 8) != 8)
                throw internal_error("error in internal synchronization mechanism: " + std::string(strerror(errno)), errno);
            std::lock_guard<std::mutex> guard(_lock);
            _shutdown_received = true;
            return nullptr;
        }

        // Check for incoming connections
        if (p[0].revents & (POLLIN | POLLERR)) {
            // Get incoming connection's address
            char addr[sizeof(struct sockaddr_in6)];
            socklen_t addrlen = sizeof(struct sockaddr_in6);
            int new_sock = ::accept(_sock, (sockaddr*)&addr, &addrlen);

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
            switch (_af) {
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

            std::unique_lock<std::mutex> guard(_lock);
            // Wait until we're under the max active connection limit, or until shutdown
            while (_active_connections.size() >= _max_active && !_shutdown_sent)
                _connections_cv.wait(guard);

            // If shutdown was sent, abort this connection attempt
            if (_shutdown_sent) {
                ::close(new_sock);
                return nullptr;
            }

            // Wrap new connection socket into the appropriate object
            tcp_server_connection* new_conn = new tcp_server_connection(this, _recv_buffer_size);
            new_conn->connect(new_sock, _server_ip, _server_port, addr_str, port);
            _active_connections.insert(new_conn);
            return new_conn;
        }
    }
}

bool tcp_server::shutdown() {
    std::lock_guard<std::mutex> guard(_lock);
    // Make sure the socket is still open
    if (_sock == -1)
        return false;
    if (_shutdown_sent)
        return true;

    // Send shutdown event
    uint64_t buf = 1;
    if (write(_event, &buf, 8) != 8)
        throw internal_error("error in internal synchronization mechanism: " + std::string(strerror(errno)), errno);
    _shutdown_sent = true;
    _connections_cv.notify_all();

    // Shutdown all connected clients
    for (auto conn : _active_connections)
        conn->shutdown();

    return true;
}

bool tcp_server::close() {
    std::unique_lock<std::mutex> guard(_lock);
    // Make sure the socket is still open
    if (_sock == -1)
        return false;

    // Make sure the socket was shut down
    if (!_shutdown_sent) {
        log.put(logging::WARNING, {"close() called without shutting down. Shutting down the server, but this may lead to a crash. If you're a plugin developer, make sure you call shutdown() on the server and wait for the accepting thread to finish, before closing."});
        shutdown();
    } else if (!_shutdown_received) {
        log.put(logging::WARNING, {"close() called before the accepting thread received the shutdown request. This may lead to instability or weird behaviour. If you're a plugin developer, make sure that you wait for the accepting thread to finish after calling shutdown(), before closing. The accepting thread should detect a shutdown when its last call to accept() returns nullptr."});
    }

    // Close socket
    ::close(_sock);
    _server_ip = "";
    _ip_family = 0;
    _server_port = 0;
    _sock = -1;

    // Wait for all connected clients to close
    while (!_active_connections.empty())
        _connections_cv.wait(guard);
    // NOTE: This might have less overhead if I add a 2nd condition variable which only triggers when active connections get to 0

    return true;
}

void tcp_server::deregister(tcp_server_connection* target) {
    std::lock_guard<std::mutex> guard(_lock);
    if (_active_connections.erase(target) == 0)
        log.put(logging::ERROR, {"Tried to deregister a connection that isn't currently this server's child."});
    _connections_cv.notify_all();
}

std::string tcp_server::server_ip() {
    std::lock_guard<std::mutex> guard(_lock);
    return _server_ip;
}

int tcp_server::server_port() {
    std::lock_guard<std::mutex> guard(_lock);
    return _server_port;
}

int tcp_server::ip_family() {
    std::lock_guard<std::mutex> guard(_lock);
    return _ip_family;
}

size_t tcp_server::recv_buffer_size() {return _recv_buffer_size;}
