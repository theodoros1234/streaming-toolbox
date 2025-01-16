#include "tcp_server.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

using namespace strtb::networking;

struct strtb::networking::tcp_server_platform_specific {
    int sock = -1;
};

tcp_server::tcp_server() : _pl(new tcp_server_platform_specific) {}

tcp_server::~tcp_server() {
    delete _pl;
}

void tcp_server::listen(const std::string& address, uint16_t port, bool reconnect, int backlog, int max_active) {
    listen(address.c_str(), port, reconnect, backlog, max_active);
}

void tcp_server::listen(const char* address, uint16_t port, bool reconnect, int backlog, int max_active) {
    std::lock_guard<std::mutex> guard(_lock);

    _backlog = backlog;
    _max_active = max_active;

    // Convert IP address to appropriate struct
    uint8_t address_struct[sizeof(struct in6_addr)] = {0};
    char address_str_clean[INET6_ADDRSTRLEN] = {0};
    int af;

    if (inet_pton(AF_INET, address, address_struct) == 1) {             // IPv4 address
        if (inet_ntop(AF_INET, address_struct, address_str_clean, INET6_ADDRSTRLEN) == NULL)
            throw internal_error(("internal error while handling IP address: " + std::string(strerror(errno))).c_str(), errno);
        _ip_family = "IPv4";
        _ip = address_str_clean;
        _port = port;
        af = AF_INET;
    } else if (inet_pton(AF_INET6, address, address_struct) == 1) {     // IPv6 address
        if (inet_ntop(AF_INET6, address_struct, address_str_clean, INET6_ADDRSTRLEN) == NULL)
            throw internal_error(("internal error while handling IP address: " + std::string(strerror(errno))).c_str(), errno);
        _ip_family = "IPv6";
        _ip = address_str_clean;
        _port = port;
        af = AF_INET6;
    } else throw address_resolution_error("given address is not a valid IPv4 or IPv6 IP address", 0);

    // Open socket
    _pl->sock = socket(af, SOCK_STREAM, 0);
    if (_pl->sock == -1) switch (errno) {
    case EACCES:
        throw connection_error(errno);
    default:
        throw internal_error(errno);
    }

    // Bind to socket
    switch (af) {
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
                throw e;
            }
        }
        break;

    default:
        throw internal_error("Unreachable code was reached, this is either a weird bug in Streaming Toolbox, or your CPU is unstable.", 0);
    }

    // printf("%s ip=%s port=%d\n", _ip_family.c_str(), _ip.c_str(), _port);

    if (::listen(_pl->sock, backlog) == -1) {
        // Error listening
        connection_error e(errno);
        ::close(_pl->sock);
        _pl->sock = -1;
        throw e;
    }
    /*
    // Parse given IP address
    struct addrinfo *gai_result, gai_hints = {
        .ai_flags = AI_NUMERICHOST | AI_NUMERICSERV | AI_ADDRCONFIG | AI_V4MAPPED | AI_PASSIVE,
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
        .ai_addrlen = 0,
        .ai_addr = NULL,
        .ai_canonname = NULL,
        .ai_next = NULL
    };

    int gai_errno = getaddrinfo(address, std::to_string(port).c_str(), &gai_hints, &gai_result);
    switch (gai_errno) {
    case 0: // Success
        break;
    case EAI_ADDRFAMILY:
    case EAI_AGAIN:
    case EAI_FAIL:
    case EAI_NODATA:
    case EAI_NONAME:
        throw address_resolution_error(gai_strerror(gai_errno), gai_errno);
    default:
        throw internal_error(gai_strerror(gai_errno), gai_errno);
    }

    for (struct addrinfo* p = gai_result; p != NULL; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            struct sockaddr_in* a = (struct sockaddr_in*) p->ai_addr;
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(a->sin_addr), ip, INET_ADDRSTRLEN);
            printf("IPv4 sin_addr=%s sin_port=%u\n", ip, ntohs(a->sin_port));
        } else {
            struct sockaddr_in6* a = (struct sockaddr_in6*) p->ai_addr;
            char ip[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &(a->sin6_addr), ip, INET6_ADDRSTRLEN);
            printf("IPv6 sin6_addr=%s sin6_port=%u sin6_flowinfo=%d sin6_scope_id=%d\n", ip, ntohs(a->sin6_port), a->sin6_flowinfo, a->sin6_scope_id);
        }
    }

    freeaddrinfo(gai_result);
    */
}
