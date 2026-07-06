#ifndef STRTB_HTTP_CLIENT_H
#define STRTB_HTTP_CLIENT_H

#include "protocol.h"
#include "logging.h"
#include "networking/tcp_client.h"
#include "networking/tcp_client_ssl.h"
#include "uri.h"
#include <variant>
#include <map>
#include <mutex>

namespace strtb::http {

class exception : public std::exception {
private:
    const std::string _what;
public:
    exception(const char *str);
    exception(const std::string &str);
    exception(std::string &&str);
    const char* what() const noexcept;
};

class bad_response : public exception {using exception::exception;};
class incomplete_data : public exception {};
class in_shutdown_state : public exception {};
class security_precaution : public exception {using exception::exception;};

class client {
private:
    std::mutex _lock;
    const std::string _log_name;
    logging::source _log;
    std::variant<bool, networking::tcp_client, networking::tcp_client_ssl> _socket_container = false;
    networking::tcp_client *_socket = nullptr;
    // TODO: rethink this, especially for persistent connections
    enum {
        STATE_IDLE,
        STATE_PREPARING,
        STATE_CONNECTING,
        STATE_RECEIVING_HEADERS,
        STATE_RECEIVING_BODY,
        STATE_DONE
    } _state = STATE_IDLE;
    bool _is_shutdown = false;
    std::string _method;
    uri::parser _url;
    std::string _hostname;
    int _port;
    std::string _path;
    bool _encrypted = false, _allow_invalid_cert = false;
    std::map<std::string, std::string> _rq_headers;
    int _status_code = 0;
    std::string _status_message;
    field_parser _rs_headers;
    http_version_ret _rs_http_version;

public:
    client(const std::string &log_name);
    client& open(const std::string &method, const std::string &url,
                 bool allow_invalid_cert = false, bool allow_unsafe_ports = false);
    client& set_header(const std::string &name, const std::string &value);
    client& set_headers(const std::map<std::string, std::string> &headers);
    bool clear_header(const std::string &name);
    int send();

    const std::string& log_name() const;
    const std::string& get_raw_header(const std::string &name) const;   // always use lowercase names
    const std::map<std::string, std::string>& get_raw_headers() const;
    int status_code() const;
    const std::string& status_message() const;
    bool is_encrypted() const;
    // TODO: more functions to get more internal variables

    void cancel();      // cancel an existing open connection
    void shutdown();    // fully shutdown current and future connections from another thread
    void reset();       // for undoing shutdown and allowing a new connection
    void clear();       // for deleting stored data
};

}

#endif // STRTB_HTTP_CLIENT_H
