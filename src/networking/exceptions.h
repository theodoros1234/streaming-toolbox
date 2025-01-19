#ifndef STRTB_NETWORKING_EXCEPTIONS_H
#define STRTB_NETWORKING_EXCEPTIONS_H

#include <string>
#include <exception>

namespace strtb::networking {

class network_error : public std::exception {};

class internal_error : public network_error {
public:
    internal_error(const char* what, int what_errno);
    internal_error(const std::string& what, int what_errno);
    internal_error(int what_errno);
    const char* what() const noexcept;
    int what_errno() const noexcept;
private:
    std::string _what;
    int _errno;
};

class address_resolution_error : public network_error {
public:
    address_resolution_error(const char* what, int what_errno);
    address_resolution_error(const std::string& what, int what_errno);
    const char* what() const noexcept;
    int what_errno() const noexcept;
private:
    std::string _what;
    int _errno;
};

class connection_error : public network_error {
public:
    connection_error(const char* what, int what_errno);
    connection_error(const std::string& what, int what_errno);
    connection_error(int what_errno);
    const char* what() const noexcept;
    int what_errno() const noexcept;
private:
    std::string _what;
    int _errno;
};

class connection_closed : public network_error {
public:
    connection_closed(const char* what, int what_errno);
    connection_closed(const std::string& what, int what_errno);
    connection_closed(int what_errno);
    const char* what() const noexcept;
    int what_errno() const noexcept;
private:
    std::string _what;
    int _errno;
};

}

#endif // STRTB_NETWORKING_EXCEPTIONS_H
