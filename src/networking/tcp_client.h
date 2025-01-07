#ifndef STRTB_NETWORKING_TCP_CLIENT_H
#define STRTB_NETWORKING_TCP_CLIENT_H

#include <cstdint>
#include <string>

namespace strtb::networking {

class network_error : public std::exception {};

class internal_error : public network_error {
public:
    internal_error(const char* what, int what_errno);
    const char* what() const noexcept;
    int what_errno() const noexcept;
private:
    std::string _what;
    int _errno;
};

class address_resolution_error : public network_error {
public:
    address_resolution_error(const char* what, int what_errno);
    const char* what() const noexcept;
    int what_errno() const noexcept;
private:
    std::string _what;
    int _errno;
};

class connection_error : public network_error {
public:
    connection_error(const char* what, int what_errno);
    const char* what() const noexcept;
    int what_errno() const noexcept;
private:
    std::string _what;
    int _errno;
};

struct tcp_client_platform_specific;

class tcp_client {
protected:
    tcp_client_platform_specific* _pl;
    char _buffer[4096];
public:
    const char* buffer = _buffer;
    tcp_client(const char* address, uint16_t port);
    tcp_client(const std::string& address, uint16_t port);
    ~tcp_client();
    ssize_t recv();
    ssize_t send(const char* buf, size_t len);
    ssize_t send(const std::string& buf);
    void shutdown();
};

}

#endif // STRTB_NETWORKING_TCP_CLIENT_H
