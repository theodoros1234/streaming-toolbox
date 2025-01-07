#include "tcp_client.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>

using namespace strtb::networking;

internal_error::internal_error(const char* what, int what_errno) : _what(what), _errno(what_errno) {}
const char* internal_error::what() const noexcept {return _what.c_str();}
int internal_error::what_errno() const noexcept {return _errno;}

address_resolution_error::address_resolution_error(const char* what, int what_errno) : _what(what), _errno(what_errno) {}
const char* address_resolution_error::what() const noexcept {return _what.c_str();}
int address_resolution_error::what_errno() const noexcept {return _errno;}

connection_error::connection_error(const char* what, int what_errno) : _what(what), _errno(what_errno) {}
const char* connection_error::what() const noexcept {return _what.c_str();}
int connection_error::what_errno() const noexcept {return _errno;}

struct strtb::networking::tcp_client_platform_specific {
    int sock = -1;
};

tcp_client::tcp_client(const char* address, uint16_t port) : _pl(new tcp_client_platform_specific) {
    // Get target host info
    struct addrinfo gai_hints = {
        .ai_flags = 0,
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = 0,
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
        delete _pl;
        throw address_resolution_error(gai_strerror(gai_err), gai_err);
    default:
        delete _pl;
        throw internal_error(gai_strerror(gai_err), gai_err);
    }

    int connect_error = 0;
    int connect_error_priority = 0;

    for (struct addrinfo* item = gai_result; item != NULL; item = item->ai_next) {
        // Try to connect to a socket through any of the returned results
        _pl->sock = socket(item->ai_family, item->ai_socktype, item->ai_protocol);
        if (_pl->sock != -1) {  // Socket creation successful
            if (connect(_pl->sock, item->ai_addr, item->ai_addrlen) == -1) {    // Connection failed
                close(_pl->sock);
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

    if (_pl->sock == -1) {  // Failed to connect
        delete _pl;
        throw connection_error(strerror(connect_error), connect_error);
    }
}

tcp_client::tcp_client(const std::string& address, uint16_t port) : tcp_client(address.c_str(), port) {};

tcp_client::~tcp_client() {
    close(_pl->sock);
    delete _pl;
}
