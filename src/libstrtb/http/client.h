#ifndef STRTB_HTTP_CLIENT_H
#define STRTB_HTTP_CLIENT_H

#include "protocol.h"
#include "codings.h"
#include "../networking/tcp_client.h"
#include "../networking/tcp_client_ssl.h"
#include "../uri.h"
#include <variant>
#include <map>
#include <mutex>

namespace strtb::http {

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

    class request {
    private:
        bool _verify_not_sending() const;
        void _with_parsed_host(bool https, std::string_view host, uri::host_type_enum type, unsigned int port);
        void _valid_state(bool running);

    protected:
        friend client;

        struct data {
            // TODO: state
            // authority: for host header, host: for socket connection
            client *c = nullptr;

            std::string method, authority, host, path;
            int port = -1;
            bool https = false, allow_invalid_cert = false, allow_unsafe_ports = false,
                 method_safe = false, method_idempotent = false,
                 path_asterisk = false, query_set = false;
            std::map<std::string, std::string> headers;
        } *_d = nullptr;

    public:
        request(std::string_view method);
        ~request();
        request(const request&) = delete;
        request(request &&other);
        request& operator=(request &&other);

        request&& with_url(std::string_view url);
        request&& with_host(bool https, std::string_view hostname);    // for IPv6, must use square brackets
        request&& with_host(bool https, std::string_view hostname, unsigned int port);
        request&& with_path(std::string_view path);
        request&& with_header(std::string_view name, std::string_view value);
        request&& with_headers(const std::map<std::string, std::string> &headers);
        request&& with_headers(const std::vector< std::pair<std::string, std::string> > &headers);
        request&& with_headers(std::initializer_list< std::pair<std::string_view, std::string_view> > headers);
        request&& allow_invalid_cert(bool value = true);
        request&& allow_unsafe_ports(bool value = true);

        // TODO: send with client?, cancel and clear
    };

    class response {
    protected:
        friend client;

        struct data {
            client *c = nullptr;

            http_version version;
            int status = 0;
            std::string status_message;
            field_parser headers, trailers;
            std::vector<std::string> headers_set_cookie;
        } *_d = nullptr;

        response(client *c);

    public:
        response() = default;
        ~response();
        response(const response&) = delete;
        response(response &&other);
        response& operator=(response &&other);

        http_version version() const;
        int status() const;
        const std::string& status_message() const;
        const std::string& header(std::string_view name) const;
        const std::map<std::string, std::string>& headers() const;
        const std::vector<std::string>& headers_set_cookie() const;
        const std::string& trailer(std::string_view name) const;
        const std::map<std::string, std::string>& trailers() const;
        std::string_view recv_body();
        std::string_view recv_body(size_t max_len);
        // TODO: cancel and clear
    };

private:
    std::mutex _lock;
    std::variant<bool, networking::tcp_client, networking::tcp_client_ssl> _socket_container = false;
    networking::tcp_client *_socket = nullptr;
    // TODO: rethink this, especially for persistent connections
    state_enum _state = STATE_IDLE;
    volatile bool _is_shutdown = false;
    std::string _authority;
    std::vector<std::unique_ptr<decoder> > _decoders;
    request::data* _request = nullptr;
    response::data* _response = nullptr;

    void _shutdown_check_early();
    void _shutdown_check();

protected:
    std::string_view recv_body();
    std::string_view recv_body(size_t max_len);

public:
    client();
    ~client();
    response send(request &r);

    state_enum state() const;
    const std::string& authority() const;
    bool encrypted() const;
    // TODO: more functions to get more internal variables

    void cancel();      // cancel an existing open connection without clearing data (NOT MT-SAFE)
    void shutdown();    // fully shutdown current and future connections from another thread
    void reset();       // for undoing shutdown and allowing a new connection
    void clear();       // for deleting stored data
};

}

#endif // STRTB_HTTP_CLIENT_H
