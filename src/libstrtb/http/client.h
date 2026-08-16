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
#include <condition_variable>
#include <poll.h>

namespace strtb::http {

class request_handler;

class client : public shutdown_controllable {
protected:
    enum recv_mode_enum {
        RECV_STREAM,
        RECV_STR
        // TODO: RECV_FILE
    };

public:
    class response : public shutdown_controllable {
    private:
        void _verify_data() const;
        void _verify_recv_mode(recv_mode_enum wanted, const char *f_name) const;
        void _shutdown();
        void _cancel();
        void _move(response &&other);

    protected:
        friend client;

        struct data {
            std::mutex lock;
            client *c = nullptr;
            shutdown_controller *shutdown_ctrl = nullptr;
            bool shutdown_ctrl_state = false;

            http_version version;
            int status = 0;
            std::string status_message;
            field_parser headers, trailers;
            recv_mode_enum recv_mode = RECV_STREAM;
            std::string body_str;
            std::variant<std::monostate, networking::tcp_client, networking::tcp_client_ssl> socket;
            std::vector<product> upgrade;
        } *_d = nullptr;

        response(client *c);
        void shutdown_controllable_signal(bool state);
        void soft_clear();

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
        std::variant<std::monostate, networking::tcp_client, networking::tcp_client_ssl> socket();
        const std::vector<product>& upgrade() const;

        void attach_shutdown_controller(shutdown_controller &ctrl);
        void detach_shutdown_controller();

        void cancel();
        void clear();
        bool empty();
    };

    class request : public shutdown_controllable {
    private:
        void _with_parsed_host(bool https, std::string_view host, uri::host_type_enum type, unsigned int port);
        void _valid_state(bool running);
        void _with_header_trust_name(const std::string &name, std::string_view value);
        template<class T> request&& _with_headers(T headers);
        template<class T> request&& _with_params(T params);
        template<class T> request&& _with_body_str(T body);
        template<class T> request&& _upgrade(T protocols);
        void _upgrade(std::string_view protocol);
        void _shutdown();
        void _cancel(std::unique_lock<std::mutex> &lock);
        void _move(request &&other);
        void _send();
        response _get_response(std::unique_lock<std::mutex> &lock);
        void _ready_to_send();

    protected:
        friend client;
        friend request_handler;

        struct data {
            std::mutex lock;
            std::condition_variable cv;
            client *c = nullptr;
            shutdown_controller *shutdown_ctrl = nullptr;
            bool shutdown_ctrl_state = false, cancelling = false;

            // authority: for host header, host: for socket connection
            std::string method, authority, host, path, body_str;
            int port = -1;
            bool https = false, allow_invalid_cert = false, allow_unsafe_ports = false,
                 method_safe = false, method_idempotent = false,
                 path_asterisk = false, query_set = false,
                 body_set = false, handler_used = false, handler_queued = false;
            recv_mode_enum recv_mode = RECV_STREAM;
            size_t recv_max_len = 0;    // only for automatic receiving
            std::map<std::string, std::string> headers;
            std::vector<product> upgrade;
            response handler_response;
            std::exception_ptr handler_exception;
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
        request&& with_body_str(std::string_view body);
        request&& with_body_str(std::string &&body);
        request&& with_shutdown_controller(shutdown_controller &ctrl);
        request&& recv_as_stream();     // default
        request&& recv_to_str();
        request&& recv_to_str(size_t max_len);
        request&& treat_as_safe();
        request&& treat_as_idempotent();
        request&& treat_as_non_idempotent();
        request&& with_auth_bearer(std::string_view token);
        request&& with_auth_basic(std::string_view username, std::string_view password);
        request&& with_auth_basic(std::string_view userinfo);
        request&& upgrade(std::string_view protocol);
        request&& upgrade(const std::vector<std::string> &protocols);
        request&& upgrade(const std::vector<std::string_view> &protocols);
        request&& upgrade(std::initializer_list<std::string_view> protocols);
        /* NOTE: when using streamed recv, make sure to pull data
         *       from the first requests to avoid blocking later ones
         */
        request&& send_async();
        response send();
        response get_response();

        void detach_shutdown_controller();

        void cancel();
        void clear();
        bool empty();
    };

private:
    std::mutex _lock;
    std::condition_variable _cv;
    std::variant<std::monostate, networking::tcp_client, networking::tcp_client_ssl> _socket_container;
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
    void _cancel_response();
    bool _connection_reusable(const std::string &authority, bool https, bool autoclose);
    void _finish_response(bool detach = false);
    void _idle_handler_attach();
    void _idle_handler_detach();
    bool _handle_response(client::response &rs, bool &retriable, const std::vector<product> &rq_upgrade);

protected:
    friend request_handler;
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

    void shutdown();        // fully shutdown current and future connections from another thread
    void reset();           // for undoing shutdown and allowing a new connection
    void wait_until_idle(); // wait until any requests and responses are done processing
};

// thrown when an upload is aborted by an error response or connection: close
class incomplete_upload : public exception {
private:
    client::response _rs;
public:
    incomplete_upload(client::response &&rs);
    incomplete_upload(incomplete_upload &other);
    incomplete_upload(incomplete_upload &&other);
    client::response& response();   // can be used with std::move(), or without for direct access
    // NOTE: when moving the response, if it's streamed, make sure to handle its body or close it to avoid stalling
};

// request creation shortcuts
client::request get();
client::request get(std::string_view url);
client::request get(bool https, std::string_view hostname);
client::request get(bool https, std::string_view hostname, unsigned int port);

client::request head();
client::request head(std::string_view url);
client::request head(bool https, std::string_view hostname);
client::request head(bool https, std::string_view hostname, unsigned int port);

client::request post();
client::request post(std::string_view url);
client::request post(bool https, std::string_view hostname);
client::request post(bool https, std::string_view hostname, unsigned int port);

client::request put();
client::request put(std::string_view url);
client::request put(bool https, std::string_view hostname);
client::request put(bool https, std::string_view hostname, unsigned int port);

client::request options();
client::request options(std::string_view url);
client::request options(bool https, std::string_view hostname);
client::request options(bool https, std::string_view hostname, unsigned int port);

// delete conflicts with keyword, so the name has to be a little awkward
client::request delete_m();
client::request delete_m(std::string_view url);
client::request delete_m(bool https, std::string_view hostname);
client::request delete_m(bool https, std::string_view hostname, unsigned int port);

}

#endif // STRTB_HTTP_CLIENT_H
