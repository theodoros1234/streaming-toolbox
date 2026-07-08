#ifndef STRTB_HTTP_CLIENT_H
#define STRTB_HTTP_CLIENT_H

#include "protocol.h"
#include "../logging.h"
#include "../networking/tcp_client.h"
#include "../networking/tcp_client_ssl.h"
#include "../uri.h"
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

class bad_state : public exception {using exception::exception;};
class bad_response : public exception {using exception::exception;};
class unsupported_response : public exception {using exception::exception;};
class incomplete_data : public exception {using exception::exception;};
class in_shutdown_state : public exception {using exception::exception;};
class security_precaution : public exception {using exception::exception;};
class premature_end : public exception {using exception::exception;};

class client {
public:
    enum state_enum {
        STATE_IDLE,
        STATE_PREPARING,
        STATE_CONNECTING,
        STATE_RECEIVING_HEADERS,
        STATE_RECEIVING_BODY,
        STATE_DONE
    };

private:
    std::mutex _lock;
    const std::string _log_name;
    logging::source _log;
    std::variant<bool, networking::tcp_client, networking::tcp_client_ssl> _socket_container = false;
    networking::tcp_client *_socket = nullptr;
    // TODO: rethink this, especially for persistent connections
    state_enum _state = STATE_IDLE;
    bool _is_shutdown = false;
    std::string _method;
    uri::parser _url;
    std::string _hostname;
    int _port = -1;
    std::string _path;
    bool _encrypted = false, _allow_invalid_cert = false;
    std::map<std::string, std::string> _rq_headers;
    int _status_code = 0;
    std::string _status_message;
    http_version _rs_http_version;
    field_parser _rs_headers = true;
    size_t _content_length = 0, _content_length_decoded = 0, _content_length_read = 0;
    bool _content_length_known = false, _content_length_decoded_known = false,
         _content_ends_on_close = false;
    std::vector<token_params> _transfer_encoding;
    std::vector<std::string> _content_encoding;

public:
    client(const std::string &log_name);
    client& open(const std::string &method, const std::string &url,
                 bool allow_invalid_cert = false, bool allow_unsafe_ports = false);
    client& set_header(const std::string &name, const std::string &value);
    client& set_headers(const std::map<std::string, std::string> &headers);
    bool clear_header(const std::string &name);
    int send();

    std::pair<const char*, size_t> recv_body();
    std::pair<const char*, size_t> recv_body(size_t max_len);

    const std::string& log_name() const;
    state_enum state() const;
    const std::string& method() const;
    const std::string& hostname() const;
    int port() const;
    const std::string& path() const;
    bool encrypted() const;
    int status_code() const;
    const std::string& status_message() const;
    http_version response_http_version() const;
    const std::string& response_header_raw(const std::string &name) const;
    const std::string* response_header_raw_or_null(const std::string &name) const;
    const std::map<std::string, std::string>& response_headers_raw() const;
    const std::vector<std::string>& response_cookies_raw() const;
    std::pair<size_t, bool> content_length() const;
    std::pair<size_t, bool> content_length_decoded() const;
    const std::vector<token_params>& transfer_encoding() const;
    const std::vector<std::string>& content_encoding() const;
    // TODO: more functions to get more internal variables

    void cancel();      // cancel an existing open connection
    void shutdown();    // fully shutdown current and future connections from another thread
    void reset();       // for undoing shutdown and allowing a new connection
    void clear();       // for deleting stored data
};

}

#endif // STRTB_HTTP_CLIENT_H
