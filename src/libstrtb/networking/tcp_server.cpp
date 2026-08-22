#include "tcp_server.h"
#include "../logging.h"
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

static logging::source log("TCP Server", false);

tcp_server::tcp_server(bool buffered_send, size_t buffer_size) : _max_active(64) {
    _buffer_size = buffer_size;
    _buffered_send = buffered_send;
    // Create new eventfd (used for shutting down server from another thread)
    _event = eventfd(0, 0);
    if (_event == -1)
        throw internal_error("failed to setup internal synchronization mechanism: " + std::string(strerror(errno)), errno);
}

tcp_server::~tcp_server() {
    try {
        detach_shutdown_controller();
        // close socket if it's still open
        // NOTE: subclasses that override close() MUST handle this on their own destructors
        if (!_socks.empty() || !_active_connections.empty()) {
            shutdown();
            close();
        }
        ::close(_event);
    } catch (std::exception &e) {
        log.critical({"Failed to destroy object: ", e.what()});
        std::terminate();
    }
}

void tcp_server::listen(const std::string& address, uint16_t port, bool reuseaddr, int backlog) {
    listen(address.c_str(), port, reuseaddr, backlog);
}

void tcp_server::listen(const char* address, uint16_t port, bool reuseaddr, int backlog) {
    std::lock_guard<std::mutex> guard(_lock);
    if (_shutdown_controller_state)
        throw connection_closed("server shut down from shutdown controller", 0);

    bound_port sock;

    // NOTE: listen should only be called from the main thread to avoid race conditions
    _shutdown_sent = false;
    _shutdown_received = false;
    sock.backlog = backlog;

    // Convert IP address to appropriate struct
    uint8_t address_struct[sizeof(struct in6_addr)] = {0};
    char address_str_clean[INET6_ADDRSTRLEN] = {0};

    if (inet_pton(AF_INET, address, address_struct) == 1) {             // IPv4 address
        if (inet_ntop(AF_INET, address_struct, address_str_clean, INET6_ADDRSTRLEN) == NULL)
            throw internal_error("internal error while handling IP address: " + std::string(strerror(errno)), errno);
        sock.ip_family = 4;
        sock.server_ip = address_str_clean;
        sock.server_port = port;
        sock.af = AF_INET;
    } else if (inet_pton(AF_INET6, address, address_struct) == 1) {     // IPv6 address
        if (inet_ntop(AF_INET6, address_struct, address_str_clean, INET6_ADDRSTRLEN) == NULL)
            throw internal_error("internal error while handling IP address: " + std::string(strerror(errno)), errno);
        sock.ip_family = 6;
        sock.server_ip = address_str_clean;
        sock.server_port = port;
        sock.af = AF_INET6;
    } else throw address_resolution_error("given address is not a valid IPv4 or IPv6 IP address", 0);

    // Open socket
    sock.sock = socket(sock.af, SOCK_STREAM, 0);
    if (sock.sock == -1) switch (errno) {
    case EACCES:
        throw connection_error(errno);
    default:
        throw internal_error(errno);
    }

    // Set reuseaddr if needed
    int value = 1;
    if (reuseaddr) {
        if (setsockopt(sock.sock, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)))
            throw internal_error(errno);
    }

    // Bind to socket
    switch (sock.af) {
    case AF_INET:
        {
            struct sockaddr_in saddr = {
                .sin_family = AF_INET,
                .sin_port = htons(port),
                .sin_addr = *((struct in_addr*) address_struct),
                .sin_zero = {0}
            };

            if (bind(sock.sock, (struct sockaddr*) &saddr, sizeof(saddr)) == -1) {
                // Error binding
                connection_error e(errno);
                ::close(sock.sock);
                throw e;
            }
        }
        break;

    case AF_INET6:
        {
            struct sockaddr_in6 saddr = {
                .sin6_family = AF_INET6,
                .sin6_port = htons(port),
                .sin6_flowinfo = 0,
                .sin6_addr = *((struct in6_addr*) address_struct),
                .sin6_scope_id = 0
            };

            int value = 1;
            // Disable dual stacking (both IPv4 and IPv6 coming in over IPv6)
            if (setsockopt(sock.sock, IPPROTO_IPV6, IPV6_V6ONLY, &value, sizeof(value))) {
                connection_error e(errno);
                ::close(sock.sock);
                throw e;
            }

            if (bind(sock.sock, (struct sockaddr*) &saddr, sizeof(saddr)) == -1) {
                // Error binding
                connection_error e(errno);
                ::close(sock.sock);
                throw e;
            }
        }
        break;

    default:
        throw internal_error("Unreachable code was reached, this is either a weird bug in Streaming Toolbox, or your CPU is unstable.", 0);
    }

    if (::listen(sock.sock, backlog) == -1) {
        // Error listening
        connection_error e(errno);
        ::close(sock.sock);
        throw e;
    }

    _socks.push_back(sock);
}

tcp_server_connection* tcp_server::accept() {
    if (_socks.empty())
        throw connection_closed("socket closed or hasn't been opened yet", 0);
    if (_shutdown_received)
        return nullptr;

    // Loop until there's a new connection, a shutdown or an error
    while (true) {
        std::vector<struct pollfd> p(_socks.size() + 1);
        // Poll listening sockets
        for (size_t i=0; i<_socks.size(); i++) {
            p.at(i) = {
                .fd = _socks.at(i).sock,
                .events = POLLIN,
                .revents = 0
            };
        }
        // Poll eventfd for shutdown
        p.at(_socks.size()) = {
            .fd = _event,
            .events = POLLIN,
            .revents = 0
        };

        // Block until there's a connection to accept, or until the server shuts down
        if (poll(p.data(), p.size(), -1) == -1)
            throw internal_error(errno);

        // Check for shutdown event
        if (p.at(_socks.size()).revents & (POLLIN | POLLERR)) {
            uint64_t buffer;
            if (read(_event, &buffer, 8) != 8)
                throw internal_error("error in internal synchronization mechanism: " + std::string(strerror(errno)), errno);
            std::lock_guard<std::mutex> guard(_lock);
            _shutdown_received = true;
            return nullptr;
        }

        // Check for incoming connections
        for (size_t i=0; i<_socks.size(); i++) {
            if (p.at(i).revents & (POLLIN | POLLERR)) {
                // Get incoming connection's address
                char addr[sizeof(struct sockaddr_in6)];
                socklen_t addrlen = sizeof(struct sockaddr_in6);
                int new_sock = ::accept(_socks.at(i).sock, (sockaddr*)&addr, &addrlen);

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
                switch (_socks.at(i).af) {
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

                // Wrap new connection socket into the appropriate object (could be a subclass for SSL)
                tcp_server_connection* new_conn = nullptr;
                try {
                    new_conn = _new_connection(_socks.at(i), new_sock, addr_str, port);
                    _active_connections.insert(new_conn);
                } catch (...) {
                    if (new_conn)
                        delete new_conn;
                    throw;
                }

                return new_conn;
            }
        }
    }
}

tcp_server_connection* tcp_server::_new_connection(const bound_port& server, int sock, std::string remote_ip, int remote_port) {
    // This is a separate function because it can be overriden by tcp_server_ssl
    return new tcp_server_connection(this, _buffered_send, _buffer_size,
                                     sock, server.server_ip, server.server_port, remote_ip, remote_port);
}

bool tcp_server::_shutdown() {
    // Make sure the socket is still open
    if (_socks.empty())
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

bool tcp_server::shutdown() {
    std::lock_guard<std::mutex> guard(_lock);
    return _shutdown();
}

bool tcp_server::close(bool pre_accept) {
    std::unique_lock<std::mutex> guard(_lock);
    // Make sure the socket is still open
    if (_socks.empty())
        return false;

    // Make sure the socket was shut down
    if (!pre_accept) {
        if (!_shutdown_sent) {
            log.put(logging::WARNING, {"close() called without shutting down. Shutting down the server, but this may lead to a crash. If you're a plugin developer, make sure you call shutdown() on the server and wait for the accepting thread to finish, before closing."});
            shutdown();
        } else if (!_shutdown_received) {
            log.put(logging::WARNING, {"close() called before the accepting thread received the shutdown request. This may lead to instability or weird behaviour. If you're a plugin developer, make sure that you wait for the accepting thread to finish after calling shutdown(), before closing. The accepting thread should detect a shutdown when its last call to accept() returns nullptr."});
        }
    }

    // Close socket
    for (bound_port& s : _socks)
        ::close(s.sock);
    _socks.clear();

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

size_t tcp_server::buffer_size() const {
    return _buffer_size;
}

bool tcp_server::buffered_send() const {
    return _buffered_send;
}

std::vector<tcp_server::bound_port> tcp_server::bound_ports() {
    std::lock_guard<std::mutex> guard(_lock);
    return _socks;
}

void tcp_server::attach_shutdown_controller(shutdown_controller &ctrl) {
    std::lock_guard<std::mutex> guard(_lock);
    if (_shutdown_controller)
        shutdown_controllable_throw_already_attached();

    _shutdown_controller_state = shutdown_controllable_attach(ctrl);
    _shutdown_controller = &ctrl;

    if (_shutdown_controller_state)
        _shutdown();
}

void tcp_server::detach_shutdown_controller() {
    shutdown_controller *p;

    {
        std::lock_guard<std::mutex> guard(_lock);
        if (!_shutdown_controller)
            return;

        p = _shutdown_controller;
        _shutdown_controller = nullptr;
        _shutdown_controller_state = false;
    }

    shutdown_controllable_detach(p);
}

void tcp_server::shutdown_controllable_signal(bool state) {
    std::lock_guard<std::mutex> guard(_lock);
    if (!_shutdown_controller)  // in the middle of detaching
        return;

    _shutdown_controller_state = state;

    if (state)
        _shutdown();
}
