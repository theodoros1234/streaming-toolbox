#ifndef STRTB_HTTP_CLIENT_H
#define STRTB_HTTP_CLIENT_H

#include "protocol.h"
#include "codings.h"
#include "../networking/tcp_client.h"
#include "../networking/tcp_client_ssl.h"
#include "../uri.h"
#include "../shutdown_controller.h"
#include <variant>
#include <map>
#include <mutex>
#include <poll.h>

namespace strtb::http {

class client : public shutdown_controllable {
protected:
    enum recv_mode_enum {
        RECV_STREAM,
        RECV_STR
        // TODO: RECV_FILE
    };

public:
    class request : public shutdown_controllable {
    private:
        bool _verify_not_sending() const;
        void _with_parsed_host(bool https, std::string_view host, uri::host_type_enum type, unsigned int port);
        void _valid_state(bool running);
        void _with_header_trust_name(const std::string &name, std::string_view value);
        template<class T> request&& _with_headers(T headers);
        template<class T> request&& _with_params(T params);
        template<class T> request&& _with_body_str(T body);
        void _shutdown();
        void _cancel();
        void _move(request &&other);

    protected:
        friend client;

        struct data {
            // TODO: state
            std::mutex lock;
            client *c = nullptr;
            shutdown_controller *shutdown_ctrl = nullptr;
            bool shutdown_ctrl_state = false;

            // authority: for host header, host: for socket connection
            std::string method, authority, host, path, body_str;
            int port = -1;
            bool https = false, allow_invalid_cert = false, allow_unsafe_ports = false,
                 method_safe = false, method_idempotent = false,
                 path_asterisk = false, query_set = false,
                 body_set = false;
            recv_mode_enum recv_mode = RECV_STREAM;
            size_t recv_max_len = 0;    // only for automatic receiving
            std::map<std::string, std::string> headers;
        } *_d = nullptr;

        void shutdown_controllable_signal(bool state);

    public:
        request() = default;
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
        request&& with_params(const std::map<std::string, std::string> &params);
        request&& with_params(const std::vector< std::pair<std::string, std::string> > &params);
        request&& with_params(std::initializer_list< std::pair<std::string_view, std::string_view> > params);
        request&& allow_invalid_cert(bool value = true);
        request&& allow_unsafe_ports(bool value = true);
        request&& with_content_type(std::string_view type);
        request&& with_body_str(const std::string &body);
        request&& with_body_str(const char *body);
        request&& with_body_str(const char *body, size_t length);
        request&& with_body_str(std::string_view &body);
        request&& with_body_str(std::string &&body);
        request&& with_shutdown_controller(shutdown_controller &ctrl);
        request&& recv_as_stream();     // default
        request&& recv_to_str();
        request&& recv_to_str(size_t max_len);

        void detach_shutdown_controller();

        void cancel();
        void clear();
    };

    class response {
    private:
        void _verify_data() const;
        void _verify_recv_mode(recv_mode_enum wanted, const char *f_name) const;

    protected:
        friend client;

        struct data {
            client *c = nullptr;

            http_version version;
            int status = 0;
            std::string status_message;
            field_parser headers, trailers;
            recv_mode_enum recv_mode = RECV_STREAM;
            std::string body_str;
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
        std::string_view recv_body(size_t max_len);     // max_len = 0 => automatically find ideal max_len
        std::string body_str();

        void cancel();
        void clear();
    };

private:
    std::mutex _lock;
    std::variant<bool, networking::tcp_client, networking::tcp_client_ssl> _socket_container = false;
    networking::tcp_client *_socket = nullptr;
    volatile bool _is_shutdown = false, _shutdown_controller_state = false,
                  _is_shutdown_rq = false, _is_shutdown_rs = false;
    shutdown_controller *_shutdown_controller = nullptr;
    std::string _authority;
    std::vector<std::unique_ptr<decoder> > _decoders;
    request::data* _request = nullptr;
    response::data* _response = nullptr;
    bool _keepalive = false;
    // for idle connection handler
    uint64_t _idle_handler_id = 0;
    size_t _idle_handler_index = 0;

    void _shutdown_check_early();
    void _shutdown_check();
    void _shutdown();
    void _reset();
    void _cancel();
    bool _connection_reusable(const std::string &authority, bool https, bool autoclose);
    void _finish_response();
    void _idle_handler_attach();
    void _idle_handler_detach();

protected:
    std::string_view recv_body(size_t max_len);
    void cancel_request();
    void cancel_response();
    void shutdown_request();
    void shutdown_response();
    void shutdown_controllable_signal(bool state);
    response send(request::data *r);

public:
    client();
    ~client();
    void attach_shutdown_controller(shutdown_controller &ctrl);
    void detach_shutdown_controller();
    response send(request &r);

    const std::string& authority() const;
    bool encrypted() const;
    // TODO: more functions to get more internal variables

    void shutdown();    // fully shutdown current and future connections from another thread
    void reset();       // for undoing shutdown and allowing a new connection
};

}

#endif // STRTB_HTTP_CLIENT_H
