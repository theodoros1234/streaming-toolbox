#ifndef STRTB_HTTP_WEBSOCKET_H
#define STRTB_HTTP_WEBSOCKET_H

#include "client.h"
#include "../networking/tcp_client_ssl.h"
#include <vector>
#include <map>
#include <initializer_list>
#include <string>
#include <string_view>
#include <variant>
#include <mutex>

namespace strtb::http {

class websocket {
public:
    class handshake_failed : public exception {
    private:
        int _status = 0;
    public:
        handshake_failed(const char *str, int status);
        handshake_failed(const std::string &str, int status);
        handshake_failed(std::string &&str, int status);
        handshake_failed(std::string_view str, int status);
        int status() const noexcept;
    };

private:
    mutable std::mutex _mutex;
    client::request _handshake_rq;
    std::vector<std::string> _subprotocols_wanted;
    std::string _subprotocol_used;
    std::variant<std::monostate, networking::tcp_client, networking::tcp_client_ssl> _socket_container;
    networking::tcp_client *_socket = nullptr;

    // TODO: check state before doing anything

    void _valid_state(bool connected);
    template<class T> websocket& _with_subprotocols(T list);

public:
    websocket();
    ~websocket();
    websocket& with_url(std::string_view url);
    websocket& with_host(bool secure, std::string_view hostname);
    websocket& with_host(bool secure, std::string_view hostname, unsigned int port);
    websocket& with_path(std::string_view path);
    websocket& with_params(const std::map<std::string, std::string> &params);
    websocket& with_params(const std::vector< std::pair<std::string, std::string> > &params);
    websocket& with_params(std::initializer_list< std::pair<std::string_view, std::string_view> > params);
    websocket& allow_invalid_cert(bool value = true);
    websocket& allow_unsafe_ports(bool value = true);
    // TODO: with_subprotocol (just one)
    websocket& with_subprotocols(const std::vector<std::string> &list);
    websocket& with_subprotocols(const std::vector<std::string_view> &list);
    websocket& with_subprotocols(std::initializer_list<std::string_view> list);
    websocket& with_header(std::string_view name, const char *value);
    websocket& with_header(std::string_view name, std::string &&value);
    websocket& with_header(std::string_view name, std::string_view value);
    websocket& with_headers(const std::map<std::string, std::string> &headers);
    websocket& with_headers(const std::vector< std::pair<std::string, std::string> > &headers);
    websocket& with_headers(std::initializer_list< std::pair<std::string_view, std::string_view> > headers);
    websocket& with_headers(std::map<std::string, std::string> &&headers);
    websocket& with_headers(std::vector< std::pair<std::string, std::string> > &&headers);
    websocket& with_auth_bearer(std::string_view token);
    websocket& with_auth_basic(std::string_view username, std::string_view password);
    websocket& with_auth_basic(std::string_view userinfo);
    // TODO: with_extensions
    void connect();

    void clear_header(std::string_view name);
    void clear_headers(std::initializer_list<std::string_view> list);
    void clear_headers();

    void clear();

    const std::string& subprotocol_used() const;
};

}

#endif // STRTB_HTTP_WEBSOCKET_H
