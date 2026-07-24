#include "client.h"
#include "strescape.h"
#include "../logging.h"

#include <assert.h>
#include <charconv>
#include <set>

#define CRLF "\r\n"

using namespace std::string_literals;

namespace strtb::http {

static logging::source log("HTTP Client", false);

static inline unsigned int default_port(bool https) {
    return https ? 443 : 80;
}

client::client() {}

client::~client() {
    if (_request) {
        log.warning_one("Destroying while a request object is still connected. "
                        "Attempting to disconnect, but this may cause a crash.");
        shutdown();
        _request->c = nullptr;
    }

    if (_response) {
        log.warning_one("Destroying while a response object is still connected. "
                        "Attempting to disconnect, but this may cause a crash.");
        shutdown();
        _response->c = nullptr;
    }
}

void client::_shutdown_check_early() {
    // try to detect a shutdown early (before the request is sent)
    if (_is_shutdown)
        throw in_shutdown_state("http client was shut down");
}

void client::_shutdown_check() {
    std::lock_guard<std::mutex> guard(_lock);
    _shutdown_check_early();
}

void client::shutdown() {
    std::lock_guard<std::mutex> guard(_lock);
    _is_shutdown = true;
    if (_socket) {
        _socket->cancel_connect();
        _socket->shutdown();
    }
}

void client::reset() {
    _is_shutdown = false;
    if (_socket)
        _socket->reset();
}

void client::_cancel() {
    std::lock_guard<std::mutex> guard(_lock);
    if (_socket) {
        if (_socket->is_open())
            _socket->close();
    }
    _decoders.clear();
}

void client::cancel_request() {
    assert(_request);
    _cancel();
    _request = nullptr;
}

void client::cancel_response() {
    assert(_response);
    _cancel();
    _response = nullptr;
}

void client::_finish_response() {
    _response = nullptr;
    if (!_keepalive)
        _socket->close();
    _decoders.clear();
}

static std::string make_header_line(const std::string &name, const std::string &value) {
    return name + ": " + value + CRLF;
    // TODO: make this send stuff directly after adding std::string_view support to tcp_socket::send();
}

bool client::_connection_reusable(const std::string &authority, bool https, bool autoclose) {
    bool sock_open = _socket && _socket->is_open();     // open connection
    bool auth_matches = _authority == authority &&      // matching authority (host and port)
                        ((https && _socket_container.index() == 2) ||   // matching encryption
                         (!https && _socket_container.index() == 1));

    // close socket if we're about to connect to another server
    if (autoclose && sock_open && !auth_matches)
        _socket->close();

    return sock_open && auth_matches;
}

client::response client::send(request &r) {
    // request must be prepared
    if (_request || _response)
        throw bad_state("another request is in progress");
    if (!r._d)
        throw std::invalid_argument("request object is empty");

    bool retriable = false;
    try {
        _request = r._d;
        _request->c = this;

        // set up the appropriate socket and connect
        bool connection_reusable = false;
        if (_request->https) {
            // https
            networking::tcp_client_ssl *s = nullptr;
            {
                std::lock_guard<std::mutex> guard(_lock);
                _shutdown_check_early();
                if (_socket_container.index() == 2) {
                    // reuse existing socket
                    connection_reusable = _connection_reusable(_request->authority, true, true);
                    _socket->reset();
                    s = &std::get<2>(_socket_container);
                } else {
                    // create required socket type
                    if (_socket && _socket->is_open())  // close the old one
                        _socket->close();
                    s = &_socket_container.emplace<2>(true);
                    _socket = s;
                    _authority.clear();
                }
            }

            if (!connection_reusable)
                s->connect(_request->host, _request->port, false, !_request->allow_invalid_cert);
        } else {
            // http
            {
                std::lock_guard<std::mutex> guard(_lock);
                _shutdown_check_early();
                if (_socket_container.index() == 1) {   // reuse existing socket
                    connection_reusable = _connection_reusable(_request->authority, false, true);
                    _socket->reset();
                } else {                                // create required socket type
                    if (_socket && _socket->is_open())  // close the old one
                        _socket->close();
                    _socket = &_socket_container.emplace<1>(true);
                    _authority.clear();
                }
            }

            if (!connection_reusable)
                _socket->connect(_request->host, _request->port);
        }

        _authority = _request->authority;

        // connection auto-retriable only if an idempotent request fails on a reused persistent connection
        retriable = connection_reusable && _request->method_idempotent;

        // send request line
        _socket->send(_request->method + " " + _request->path + " HTTP/1.1" CRLF);

        // decide connection options
        // std::string rq_connection_options = "keep-alive";
        // TODO: option to close, and include TE, Upgrade, etc. when necessary
        _keepalive = true;

        // send headers, prioritizing some, and with default values
        // WARNING: always use lowercase names, and never use values that may contain CRLF
        std::initializer_list< std::pair<std::string, std::string> > priority_headers = {
            {"host"s, _authority},
            {"user-agent"s, get_default_user_agent()},
            // {"connection"s, std::move(rq_connection_options)},
            {"accept-encoding"s, get_supported_decoders_str()}
        };

        for (const auto &h : priority_headers) {
            auto h_existing = _request->headers.find(h.first);
            if (h_existing == _request->headers.end()) {
                // send the default value we defined above
                _socket->send(make_header_line(h.first, h.second));
            } else {
                // send existing header
                _socket->send(make_header_line(h_existing->first, h_existing->second));
                _request->headers.erase(h_existing);
            }
        }

        // send remaining headers
        for (const auto &h : _request->headers)
            _socket->send(make_header_line(h.first, h.second));

        // empty line to mark end of headers
        _socket->send(CRLF);
        _socket->flush();

        // create response object
        response rs(this);
        _response = rs._d;

        // receive response status-line
        // request stops being retriable as soon as any data is received
        std::string line;
        if (!_socket->recv_line(line, true, CRLF, STRTB_HTTP_STATUS_LINE_MAX_LEN)) {
            if (line.length() == STRTB_HTTP_STATUS_LINE_MAX_LEN) {
                retriable = false;
                throw invalid_message("response status line too long");
            } else if (line.empty()) {
                throw incomplete_message("server closed the connection before anything was received");
            } else {
                retriable = false;
                throw invalid_message("incomplete response status line");
            }
        }
        retriable = false;

        auto status_line = parse_status_line(line);
        if (!status_line.valid)
            throw invalid_message("invalid response status line");
        if (status_line.version.major != 1)
            throw invalid_message("incompatible response HTTP version");
        _response->status = status_line.status_code;
        _response->status_message = std::move(status_line.reason_phrase);
        _response->version = status_line.version;

        // receive response headers
        while (true) {
            // TODO: obs-fold is allowed, give a param to enable/disable that
            // TODO: limit max amount of headers and trailers to receive

            if (!_socket->recv_line(line, true, CRLF, STRTB_HTTP_FIELD_LINE_MAX_LEN)) {
                if (line.length() >= STRTB_HTTP_FIELD_LINE_MAX_LEN)
                    throw unsupported_message("response header line too long");
                else
                    throw invalid_message("incomplete response header line");
            }

            // empty line marks end of headers
            if (line.empty())
                break;

            if (!_response->headers.process_line(line))
                throw invalid_message("invalid response header line");
        }

        // check connection options
        std::set<std::string> rs_connection_options;
        auto rs_connection_options_header = _response->headers.fields.find("connection");
        if (rs_connection_options_header != _response->headers.fields.end()) {
            auto rs_connection_options_list = parse_field_token_list(rs_connection_options_header->second, false);
            if (!rs_connection_options_list.valid)
                throw invalid_message("invalid response connection header");

            for (const auto &opt : rs_connection_options_list.list)
                rs_connection_options.insert(std::move(opt));
        }

        // determine if the connection can be reused
        if (rs_connection_options.count("close") || _response->version.minor < 1)
            _keepalive = false;

        // TODO: Transfer-Encoding in HTTP/1.0 MUST be treated as faulty framing and close the connection afterwards

        // determine if a body is present
        if (_request->method == "HEAD" || _response->status == 204 ||
            _response->status == 304 || _response->status / 100 == 1) {
            // certain methods and status codes cannot have a body
            // close connection if there's no body
            // TODO: remove this when persistent connections are implemented
            _response->c = nullptr;
            _finish_response();
        } else {
            auto te = _response->headers.fields.find("transfer-encoding");
            if (te != _response->headers.fields.end()) {
                // parse transfer-encoding header
                auto te_parsed = parse_field_token_params_list(te->second, false, true);
                if (!te_parsed.valid)
                    throw invalid_message("invalid response transfer encoding");

                // TODO: check return value to decide if the connection needs to close afterwards
                if (!transfer_encoding_make_decoders(_decoders, te_parsed.list, _response->trailers, *_socket, true))
                    _keepalive = false;
            } else {
                auto ce = _response->headers.fields.find("content-length");

                if (ce != _response->headers.fields.end()) {
                    // parse content-length header
                    auto ce_parsed = parse_field_integer(ce->second);
                    if (!ce_parsed.valid) {
                        if (ce_parsed.overflow)
                            throw unsupported_message("response content length is too long");
                        else
                            throw invalid_message("invalid response content length");
                    }

                    _decoders.push_back(std::unique_ptr<decoder>(new body_fixed_length(*_socket, ce_parsed.number)));
                } else {
                    // no encoding or length info
                    _decoders.push_back(std::unique_ptr<decoder>(new body_until_close(*_socket)));
                    _keepalive = false;
                }
            }

            // check, parse and handle content-encoding
            auto ce = _response->headers.fields.find("content-encoding");
            if (ce != _response->headers.fields.end()) {
                auto ce_parsed = parse_field_token_list(ce->second, false);
                if (!ce_parsed.valid)
                    throw invalid_message("invalid response content encoding");

                content_encoding_make_decoders(_decoders, ce_parsed.list);
            }
        }

        _request->c = nullptr;
        _request = nullptr;

        return rs;
    } catch (in_shutdown_state&) {
        r._d->c = nullptr;
        cancel_request();
        throw;
    } catch (...) {
        r._d->c = nullptr;
        cancel_request();
        // check if the error was caused by a shutdown
        _shutdown_check();

        // auto-retry if it's safe to do so
        if (retriable)
            return send(r);
        else
            throw;
    }
}

std::string_view client::recv_body() {
    return recv_body(_socket->buffer_size());
    // TODO: determine what's an actual good default max_len
}

std::string_view client::recv_body(size_t max_len) {
    assert(_response);

    try {
        assert(!_decoders.empty());
        if (!_decoders.empty()) {
            auto [data, len] = _decoders.back()->read(max_len);

            if (len == 0) {     // reading 0 bytes means we reached the end of the body
                // unless a shutdown truncated part of the body and somehow didn't cause an error
                _shutdown_check();
                _finish_response();
                // TODO: attempt graceful shutdown over TLS
            }

            return std::string_view(data, len);
        } else {
            return std::string_view();
        }
    } catch (in_shutdown_state&) {
        cancel_response();
        throw;
    } catch (...) {
        cancel_response();
        // check if the error was caused by a shutdown
        _shutdown_check();
        throw;
    }
}

const std::string& client::authority() const {
    return _authority;
}

bool client::encrypted() const {
    return _socket_container.index() == 2;
}

client::request::request(std::string_view method) {
    _d = new data;

    // check validity
    auto [method_to, method_valid] = parse_token(method);
    if (!method_valid || method_to != method.length())
        throw std::invalid_argument("invalid request method");
    _d->method = method;

    // check if it's safe and/or idempotent
    // TODO: faster string matching
    _d->method_safe = _d->method == "GET" || _d->method == "HEAD" || _d->method == "OPTIONS" || _d->method == "TRACE";
    _d->method_idempotent = _d->method_safe || _d->method == "PUT" || _d->method == "DELETE";
}

client::request::~request() {
    cancel();
    if (_d) {
        delete _d;
        _d = nullptr;
    }
}

client::request::request(request &&other) {
    // WARNING: DO NOT run this while using the other request from another thread
    _d = other._d;
    other._d = nullptr;
}

client::request& client::request::operator=(request &&other) {
    cancel();
    if (_d)
        delete _d;

    _d = other._d;
    other._d = nullptr;

    return *this;
}

void client::request::cancel() {
    if (_d && _d->c) {
        _d->c->cancel_request();
        _d->c = nullptr;
    }
}

void client::request::clear() {
    cancel();
    if (_d) {
        delete _d;
        _d = nullptr;
    }
}

void client::request::_valid_state(bool running) {
    // make sure this request is in a valid state (not moved or (not) in-progress)
    if (!_d)
        throw bad_state("cannot reuse request after it has been moved");

    if ((_d->c != nullptr) != running) {
        if (running)
            throw bad_state("request not in progress");
        else
            throw bad_state("request already in progress");
    }
}

client::request&& client::request::with_url(std::string_view url) {
    _valid_state(false);

    try {
        uri::parser parser;

        // parse URL
        auto [url_to, url_valid] = parser.parse_uri(url, false);
        if (!url_valid || parser.path_type != uri::PATH_ABEMPTY)
            throw std::invalid_argument("invalid or incompatible URL "
                                        "(note that IPv6 addresses must be enclosed in square brackets)");

        // check scheme
        _d->https = false;
        std::string scheme = parser.scheme_str(url, true);
        if (scheme == "https")
            _d->https = true;
        else if (scheme != "http")
            throw std::invalid_argument("incompatible URL scheme, only http/https allowed");

        // check host and port
        std::string host = parser.host_str(url, true);
        int port = -1;
        if (parser.port_from == parser.port_to) // no port specified, use default
            port = default_port(_d->https);
        else    // a port was specified
            port = parser.port_uint16(url);     // -1 error will be handled by _d->with_parsed_host
        _with_parsed_host(_d->https, host, parser.host_type, port);

        // path
        // TODO: handle any double slashes and dot segments
        _d->path_asterisk = false;
        // empty path corresponds to /
        _d->path = (parser.path_from == parser.path_to) ? "/" : parser.path_str(url);

        // query
        _d->query_set = parser.query_to;
        if (_d->query_set) {
            _d->path += '?';
            _d->path.append(url.data() + parser.query_from, parser.query_to - parser.query_from);
        }

        // fragment is ignored
    } catch (...) {
        _d->https = false;
        _d->port = -1;
        _d->path_asterisk = false;
        _d->query_set = false;
        _d->authority.clear();
        _d->host.clear();
        _d->path.clear();
        throw;
    }

    return std::move(*this);
}

// checks the host and port, stores them, and creates authority string (https must be stored by caller)
void client::request::_with_parsed_host(bool https, std::string_view host, uri::host_type_enum type, unsigned int port) {
    // check host
    switch (type) {
    case uri::HOST_REGNAME:
    case uri::HOST_IPV4:
        _d->host = host;
        break;

    case uri::HOST_IPV6:
        _d->host = host.substr(1, host.size() - 2);    // trim square brackets
        break;

    case uri::HOST_EMPTY:
        throw std::invalid_argument("missing host");

    case uri::HOST_IPVFUTURE:
    default:
        throw std::invalid_argument("invalid or unsupported IP address type");
    }

    // check port range
    if (port >= 65536)
        throw std::invalid_argument("invalid port number");
    // check for unsafe ports
    if (!_d->allow_unsafe_ports && _d->port != 80 && port != 443 && is_unsafe_port(port))
        throw security_precaution("blocked access to unsafe port");
    _d->port = port;

    // set authority (used by host header)
    if (port == default_port(https)) {  // no need to specify the default port
        _d->authority = host;
    } else {
        // convert port to str without locale issues
        char port_str[6] = {0};
        std::to_chars(port_str, &port_str[sizeof(port_str) - 1], port);
        _d->authority = host + ":" + port_str;
    }
}

client::request&& client::request::with_host(bool https, std::string_view hostname) {
    return with_host(https, hostname, default_port(https));
}

client::request&& client::request::with_host(bool https, std::string_view hostname, unsigned int port) {
    _valid_state(false);

    try {
        _d->https = https;

        // parse host
        uri::parser parser;
        auto [hostname_to, hostname_valid] = parser.parse_host(hostname, false);
        if (!hostname_valid)
            throw std::invalid_argument("invalid hostname or IP address "
                                        "(note that IPv6 addresses must be enclosed in square brackets)");
        std::string host_str = parser.host_str(hostname, true);

        _with_parsed_host(https, host_str, parser.host_type, port);
    } catch (...) {
        _d->https = false;
        _d->port = -1;
        _d->host.clear();
        _d->authority.clear();
        throw;
    }

    return std::move(*this);
}

// NOTE: only allows origin form and asterisk form, maybe change this later if it's a problem
client::request&& client::request::with_path(std::string_view path) {
    _valid_state(false);

    try {
        _d->query_set = false;
        _d->path_asterisk = (path.size() == 1 && path[0] == '*');

        if (_d->path_asterisk) {   // asterisk-form (used for OPTIONS requests)
            _d->path = path;
        } else {    // origin-form (used by most methods)
            uri::parser parser;

            // handle path
            if (!parser.parse_relative_ref(path, false).second)
                throw std::invalid_argument("invalid or unsupported path");

            switch (parser.path_type) {
            case uri::PATH_ABSOLUTE:
                _d->path = parser.path_str(path);
                break;

            case uri::PATH_EMPTY:   // empty path corresponds to /
                _d->path = "/";
                break;

            default:
                throw std::invalid_argument("invalid or unsupported path");
            }

            // handle query
            if (parser.query_to) {
                _d->query_set = true;
                _d->path += '?';
                _d->path.append(path.data() + parser.query_from, parser.query_to - parser.query_from);
            }

            // ignore fragment
        }
    } catch (...) {
        _d->query_set = false;
        _d->path_asterisk = false;
        _d->path.clear();
        throw;
    }

    return std::move(*this);
}

client::request&& client::request::with_header(std::string_view name, std::string_view value) {
    _valid_state(false);

    // case-insensitive name
    std::string name_tolower = parse_token_tolower(name);
    if (name_tolower.empty() || name_tolower.length() != name.length())
        throw std::invalid_argument("invalid header name");

    // check value for invalid characters
    for (char c : value)
        if (!(is_vchar(c) || is_obs_text(c) || is_whitespace(c)))
            std::invalid_argument("value contains invalid character " + char_escape(c));

    try {
        _d->headers[name_tolower] = value;
    } catch (...) {
        _d->headers.erase(name_tolower);
        throw;
    }

    return std::move(*this);
}

template<class T> client::request&& client::request::_with_headers(T headers) {
    _valid_state(false);
    _d->headers.clear();

    try {
        // replace all headers with the new header list (duplicates will be silently ignored)
        for (const auto& [name, value] : headers)
            with_header(name, value);
    } catch (...) {
        _d->headers.clear();
        throw;
    }

    return std::move(*this);
}

client::request&& client::request::with_headers(const std::map<std::string, std::string> &headers) {
    return _with_headers<const std::map<std::string, std::string> &>(headers);
}

client::request&& client::request::with_headers(const std::vector< std::pair<std::string, std::string> > &headers) {
    return _with_headers<const std::vector< std::pair<std::string, std::string> > &>(headers);
}

client::request&& client::request::with_headers(
        std::initializer_list< std::pair<std::string_view, std::string_view> > headers) {
    return _with_headers<std::initializer_list< std::pair<std::string_view, std::string_view> > >(headers);
}

// URL params, automatically percent-escape reserved characters
template<class T> client::request&& client::request::_with_params(T params) {
    _valid_state(false);
    if (_d->path.empty())
        throw bad_state("cannot set params before setting path");
    if (_d->path_asterisk)
        throw bad_state("cannot set params for asterisk path");
    if (_d->query_set)
        throw bad_state("query segment already set");

    // create query str from these params
    std::string query_str = "?";
    bool first = true;

    for (const auto &param : params) {
        if (first) {
            first = false;
        } else {
            // & delimiter
            query_str += '&';
        }

        // key
        query_str += uri::percent_encode(param.first);

        // = delimiter
        query_str += '=';

        // value
        query_str += uri::percent_encode(param.second);
    }

    _d->path += query_str;
    _d->query_set = true;

    return std::move(*this);
}

client::request&& client::request::with_params(const std::map<std::string, std::string> &params) {
    return _with_params<const std::map<std::string, std::string>&>(params);
}

// multiple instances of the same param allowed
client::request&& client::request::with_params(const std::vector< std::pair<std::string, std::string> > &params) {
    return _with_params<const std::vector< std::pair<std::string, std::string> >&>(params);
}

// multiple instances of the same param allowed
client::request&& client::request::with_params(std::initializer_list< std::pair<std::string_view, std::string_view> > params) {
    return _with_params<std::initializer_list< std::pair<std::string_view, std::string_view> > >(params);
}

client::request&& client::request::allow_invalid_cert(bool value) {
    _valid_state(false);
    _d->allow_invalid_cert = value;
    return std::move(*this);
}

client::request&& client::request::allow_unsafe_ports(bool value) {
    _valid_state(false);
    _d->allow_unsafe_ports = value;
    return std::move(*this);
}

client::response::response(client *c) {
    _d = new data;
    _d->c = c;
}

client::response::~response() {
    cancel();
    if (_d) {
        delete _d;
        _d = nullptr;
    }
}

client::response::response(response &&other) {
    _d = other._d;
    other._d = nullptr;
}

client::response& client::response::operator=(response &&other) {
    cancel();
    if (_d)
        delete _d;

    _d = other._d;
    other._d = nullptr;

    return *this;
}

http_version client::response::version() const {
    return _d->version;
}

int client::response::status() const {
    return _d->status;
}

const std::string& client::response::status_message() const {
    return _d->status_message;
}

const std::string& client::response::header(std::string_view name) const {
    return _d->headers.get_field(name);
}

const std::map<std::string, std::string>& client::response::headers() const {
    return _d->headers.fields;
}

const std::vector<std::string>& client::response::headers_set_cookie() const {
    return _d->headers_set_cookie;
}

const std::string& client::response::trailer(std::string_view name) const {
    return _d->trailers.get_field(name);
}

const std::map<std::string, std::string>& client::response::trailers() const {
    return _d->trailers.fields;
}

// TODO: mostly duplicate code, tidy this up
std::string_view client::response::recv_body() {
    if (!_d)
        throw bad_state("no response assigned");
    if (!_d->c)
        return std::string_view();

    try {
        auto ret = _d->c->recv_body();
        if (ret.empty())
            _d->c = nullptr;
        return ret;
    } catch (...) {
        _d->c = nullptr;
        throw;
    }
}

std::string_view client::response::recv_body(size_t max_len) {
    if (!_d)
        throw bad_state("no response assigned");
    if (!_d->c)
        return std::string_view();

    try {
        auto ret = _d->c->recv_body(max_len);
        if (ret.empty())
            _d->c = nullptr;
        return ret;
    } catch (...) {
        _d->c = nullptr;
        throw;
    }
}

void client::response::cancel() {
    if (_d && _d->c) {
        _d->c->cancel_response();
        _d->c = nullptr;
    }
}

void client::response::clear() {
    cancel();
    if (_d) {
        delete _d;
        _d = nullptr;
    }
}

}
