#include "tcp_client.h"
#include "../logging/logging.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cstring>

// Platform-specific
#ifdef __linux__
#include <sys/eventfd.h>
#include <poll.h>
#include <fcntl.h>
#endif

using namespace strtb::networking;

static strtb::logging::source log("TCP Client");

tcp_client::tcp_client() : tcp_socket() {
    // Create new eventfd (used for shutting down server from another thread)
    _event = eventfd(0, 0);
    if (_event == -1)
        throw internal_error("failed to setup internal synchronization mechanism: " + std::string(strerror(errno)), errno);
}

tcp_client::~tcp_client() {
    if (_sock != -1)
        log.put(logging::WARNING, {"Destructor called when client connection to ", _remote_ip, ":", _remote_port, " was still open. Closing the socket, but this may lead to a crash. If you're a plugin developer, make sure you call close() on the socket."});
    ::close(_event);
}

void tcp_client::connect(const char* address, uint16_t port, time_t timeout) {
    int sock_tmp = -1;
    {
        std::lock_guard<std::recursive_mutex> guard(_lock);
        if (_sock != -1)
            throw connection_error("socket already connected", EISCONN);
        else if (_connecting)
            throw connection_error("connect was called by another thread", EALREADY);
        else if (_connect_restrict)
            throw connection_closed("Connection cancelled", 0);
        _connecting = true;
        _cancel_sent = false;
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
    int gai_err = getaddrinfo(address, std::to_string(port).c_str(), &gai_hints, &gai_result);
    if (gai_err != 0) {
        std::lock_guard<std::recursive_mutex> guard(_lock);
        _connecting = false;

        switch (gai_err) {
        case EAI_ADDRFAMILY:
        case EAI_AGAIN:
        case EAI_FAIL:
        case EAI_NODATA:
        case EAI_NONAME:
            throw address_resolution_error(gai_strerror(gai_err), gai_err);
        default:
            throw internal_error(gai_strerror(gai_err), gai_err);
        }
    }

    connection_error connect_error = 0;
    int connect_error_priority = 0;

    struct addrinfo* item = NULL;
    for (item = gai_result; item != NULL; item = item->ai_next) {
        // Try to connect to a socket through any of the returned results
        sock_tmp = socket(item->ai_family, item->ai_socktype, item->ai_protocol);
        if (sock_tmp != -1) {  // Socket creation successful
            try {
                // Asynchronously start connection
                int flags = fcntl(sock_tmp, F_GETFL);
                if (flags == -1)
                    throw internal_error(errno);
                flags |= O_NONBLOCK;
                if (fcntl(sock_tmp, F_SETFL, flags))
                    throw internal_error(errno);

                if (::connect(sock_tmp, item->ai_addr, item->ai_addrlen) == 0)      // Connection was instantly successful
                    break;
                else if (errno != EINPROGRESS)                                      // Connection instantly failed
                    throw connection_error(errno);

                // Wait for a cancellation signal or for the connection process to finish
                struct pollfd p[] = {
                    {
                        .fd = sock_tmp,
                        .events = POLLOUT,
                        .revents = 0
                    }, {
                        .fd = _event,
                        .events = POLLIN,
                        .revents = 0
                    }
                };

                flags &= ~O_NONBLOCK;
                if (fcntl(sock_tmp, F_SETFL, flags) && errno != EINPROGRESS)
                    throw internal_error(errno);

                int poll_return = poll(p, 2, timeout * 1000);
                if (poll_return == -1)
                    throw internal_error(errno);
                else if (poll_return == 0)
                    throw connection_error(ETIMEDOUT);

                if (p[1].revents) {     // Check cancellation
                    uint64_t buffer;
                    ::close(sock_tmp);
                    sock_tmp = -1;
                    freeaddrinfo(gai_result);

                    {
                        std::lock_guard<std::recursive_mutex> guard(_lock);
                        _connecting = false;
                    }

                    if (read(_event, &buffer, 8) != 8) {
                        int event_errno = errno;
                        sock_tmp = -1;
                        throw internal_error("error in internal synchronization mechanism: " + std::string(strerror(event_errno)), event_errno);
                    } else throw connection_closed("Connection cancelled", 0);
                }

                int sock_err;
                if (p[0].revents) {     // Check connection
                    socklen_t sock_err_len = sizeof(int);
                    if (getsockopt(sock_tmp, SOL_SOCKET, SO_ERROR, &sock_err, &sock_err_len)) {
                        int gso_errno = errno;
                        ::close(sock_tmp);
                        sock_tmp = -1;
                        throw internal_error("getsockopt: " + std::string(strerror(errno)), gso_errno);
                    }

                    if (sock_err)       // Connection error
                        throw connection_error(sock_err);
                    else                // Connection successful
                        break;
                }

                // Unreachable code, poll returned without anything happening
                ::close(sock_tmp);
                sock_tmp = -1;
                {
                    std::lock_guard<std::recursive_mutex> guard(_lock);
                    _connecting = false;
                }
                throw internal_error("unreachable code reached: poll returned when nothing happened", 0);

            } catch (connection_error &e) {
                ::close(sock_tmp);
                sock_tmp = -1;

                // Determine what went wrong
                // Out of all address connections, only the most "important" error will be given back (if all fail).
                switch (e.what_errno()) {
                case ENETUNREACH:
                    if (connect_error_priority < 1) {
                        connect_error = e;
                        connect_error_priority = 1;
                    }
                    break;

                case ETIMEDOUT:
                    if (connect_error_priority < 2) {
                        connect_error = e;
                        connect_error_priority = 2;
                    }
                    break;

                case ECONNREFUSED:
                    if (connect_error_priority < 3) {
                        connect_error = e;
                        connect_error_priority = 3;
                    }
                    break;

                case EACCES:
                case EPERM:
                    if (connect_error_priority < 4) {
                        connect_error = e;
                        connect_error_priority = 4;
                    }
                    break;

                default:
                    if (connect_error_priority < 5) {
                        connect_error = e;
                        connect_error_priority = 5;
                    }
                }

            } catch (internal_error &e) {
                /*
                 * These kinds of errors are considered critical and should never happen,
                 * unless the code has bugs or the application or OS reached an unstable state,
                 * so we just close the socket and give up when this happens.
                 */
                ::close(sock_tmp);
                sock_tmp = -1;
                freeaddrinfo(gai_result);
                {
                    std::lock_guard<std::recursive_mutex> guard(_lock);
                    _connecting = false;
                }
                throw;
            }
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

    std::lock_guard<std::recursive_mutex> guard(_lock);
    _connecting = false;
    _sock = sock_tmp;

    // Store remote host's IP and port
    if (sock_tmp != -1) {
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

    if (sock_tmp == -1)    // Failed to connect
        throw connection_error(connect_error);
}

void tcp_client::connect(const std::string& address, uint16_t port, time_t timeout) {
    connect(address.c_str(), port, timeout);
}

void tcp_client::reset() {
    std::lock_guard<std::recursive_mutex> guard(_lock);
    _connect_restrict = false;
}

void tcp_client::cancel_connect() {
    std::lock_guard<std::recursive_mutex> guard(_lock);
    _connect_restrict = true;
    if (_connecting && !_cancel_sent) {
        uint64_t buf = 1;
        if (write(_event, &buf, 8) != 8)
            throw internal_error("error in internal synchronization mechanism: " + std::string(strerror(errno)), errno);
        _cancel_sent = true;
    }
}

void tcp_client::close() {
    std::lock_guard<std::recursive_mutex> guard(_lock);
    cancel_connect();
    _remote_ip = "";
    _remote_port = 0;
    tcp_socket::close();
}

std::string tcp_client::remote_ip() {
    std::lock_guard<std::recursive_mutex> guard(_lock);
    return _remote_ip;
}

int tcp_client::remote_port() {
    std::lock_guard<std::recursive_mutex> guard(_lock);
    return _remote_port;
}

bool tcp_client::is_connecting() {
    std::lock_guard<std::recursive_mutex> guard(_lock);
    return _connecting;
}
