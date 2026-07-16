#include "client.h"
#include "strescape.h"

#include <assert.h>
#include <charconv>

#define CRLF "\r\n"

using namespace std::string_literals;

namespace strtb::http {

static inline unsigned int default_port(bool https) {
    return https ? 443 : 80;
}

client::client(const std::string &log_name) :
    _log_name(log_name),
    _log(log_name.empty() ? "HTTP Client" : "HTTP Client: " + log_name, false) {}

void client::_shutdown_check_early() {
    // try to detect a shutdown early (before the request is sent)
    if (_is_shutdown)
        throw in_shutdown_state("http client was shut down");
}

void client::_shutdown_check() {
    std::lock_guard<std::mutex> guard(_lock);
    _shutdown_check_early();
}

client& client::open(const std::string &method, const std::string &url, bool allow_invalid_cert, bool allow_unsafe_ports) {
    clear();
    _shutdown_check_early();

    try {
        _state = STATE_PREPARING;

        // check given method
        auto [method_to, method_valid] = parse_token(method);
        if (!method_valid || method_to != method.length())
            throw std::invalid_argument("invalid request method");
        _method = method;

        // check given URL
        auto [url_to, url_valid] = _url.parse_uri(url, true);
        if (!url_valid || _url.path_type != uri::PATH_ABEMPTY)
            throw std::invalid_argument("invalid or incompatible URL");
        if (_url.fragment_to != 0)
            throw std::invalid_argument("request URL cannot have a fragment");

        // check URL scheme
        std::string scheme = _url.scheme_str(true);
        if (scheme == "https") {
            _encrypted = true;
            _allow_invalid_cert = allow_invalid_cert;
        } else if (scheme != "http") {
            throw std::invalid_argument("incompatible URL scheme, only http/https allowed");
        }

        // check URL host
        std::string host = _url.host_str(true);
        switch (_url.host_type) {
        case uri::HOST_REGNAME:
        case uri::HOST_IPV4:
            _hostname = host;
            break;

        case uri::HOST_IPV6:
            _hostname = host.substr(1, _url.host_to - _url.host_from - 2);  // trim square brackets
            break;

        case uri::HOST_EMPTY:
            throw std::invalid_argument("missing host");

        case uri::HOST_IPVFUTURE:
        default:
            throw std::invalid_argument("invalid or unsupported IP address type");
        }

        // check URL port
        if (_url.port_to == _url.port_from) {   // unspecified port, use default
            _port = _encrypted ? 443 : 80;
        } else {    // specified port, check its validity
            _port = _url.port_uint16();
            if (_port == -1)
                throw std::invalid_argument("invalid port number");
            else if (!allow_unsafe_ports && _port != 80 && _port != 443 && is_unsafe_port(_port))
                throw security_precaution("blocked access to unsafe port");
        }

        // disallow fragments
        if (_url.fragment_to)
            throw std::invalid_argument("URL cannot contain fragment");

        // extract path (replace empty path with /)
        _path = (_url.path_to == _url.path_from) ? "/" : _url.path_str();

        // merge query with path
        if (_url.query_to)
            _path += _url.query_str();

        // set host header
        if ((_encrypted && _port == 443) || (!_encrypted && _port == 80))   // default port, omit from header
            set_header("host", host);
        else    // other port, specify it
            set_header("host", host + ":" + std::to_string(_port)); // TODO: possible issues with certain locales

        // set some headers
        set_header("user-agent", get_default_user_agent());
        set_header("connection", "close");
        set_header("accept-encoding", "gzip, deflate, br, zstd");   // TODO: get supported encodings from elsewhere
    } catch (...) {
        clear();
        throw;
    }

    return *this;
}

client& client::set_header(const std::string &name, const std::string &value) {
    _shutdown_check_early();

    // must be in preparation state
    if (_state != STATE_PREPARING) {
        if (_state < STATE_PREPARING)
            throw bad_state("cannot set request headers before opening a new request");
        else
            throw bad_state("cannot set request headers after the request was already sent");
    }

    // name must be case-insensitive
    std::string name_tolower = parse_token_tolower(name);
    if (name_tolower.empty() || name_tolower.length() != name.length())
        throw std::invalid_argument("invalid header name");

    // check value for invalid characters
    for (char c : value)
        if (!(is_vchar(c) || is_obs_text(c) || is_whitespace(c)))
            std::invalid_argument("value contains invalid character " + char_escape(c));

    _rq_headers[name_tolower] = value;

    return *this;
}

client& client::set_headers(const std::map<std::string, std::string> &headers) {
    _shutdown_check_early();

    try {
        for (const auto &[name, value] : headers) {
            try {
                set_header(name, value);
            } catch (const std::invalid_argument &e) {
                // attach header name to certain exceptions
                throw std::invalid_argument(name + ": " + e.what());
            }
        }
    } catch (...) {
        // safer to clear, cause caller doesn't know which header caused the exception
        clear();
        throw;
    }

    return *this;
}

bool client::clear_header(const std::string &name) {
    // must be in preparation state
    if (_state != STATE_PREPARING) {
        if (_state < STATE_PREPARING)
            throw bad_state("cannot set request headers before opening a new request");
        else
            throw bad_state("cannot set request headers after the request was already sent");
    }

    std::string name_tolower = parse_token_tolower(name);
    if (!name_tolower.empty() && name_tolower.length() == name.length())
        return _rq_headers.erase(name_tolower);
    return false;
}

void client::clear() {
    // cancel any open connection
    cancel();
    _state = STATE_IDLE;

    // clear request data
    _method.clear();
    _url.clear();
    _hostname.clear();
    _port = -1;
    _path.clear();
    _encrypted = false;
    _allow_invalid_cert = false;
    _rq_headers.clear();

    // clear response data
    _status_code = 0;
    _status_message.clear();
    _rs_headers.clear();
    _rs_trailers.clear();
    _rs_http_version.major = 0;
    _rs_http_version.minor = 0;
    _content_length = 0;
    _content_length_known = false;
    _transfer_encoding.clear();
    _content_encoding.clear();
    _decoders.clear();
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
    clear();
    _is_shutdown = false;
}

void client::cancel() {
    std::lock_guard<std::mutex> guard(_lock);
    if (_socket) {
        if (_socket->is_open())
            _socket->close();
        _socket = nullptr;
        _socket_container.emplace<0>(false);
    }

    if (STATE_CONNECTING <= _state && _state < STATE_DONE)
        _state = STATE_DONE;
}

static std::string make_header_line(const std::string &name, const std::string &value) {
    return name + ": " + value + CRLF;
}

int client::send() {
    // request must be prepared
    if (_state != STATE_PREPARING) {
        if (_state < STATE_PREPARING)
            throw bad_state("request not prepared");
        else
            throw bad_state("request already sent");
    }

    try {
        assert(_socket == nullptr);
        _state = STATE_CONNECTING;

        // set up the appropriate socket and connect
        if (_encrypted) {
            // https
            networking::tcp_client_ssl *s = nullptr;
            {
                std::lock_guard<std::mutex> guard(_lock);
                _shutdown_check_early();
                s = &_socket_container.emplace<2>(true);
                _socket = s;
            }
            s->connect(_hostname, _port, false, !_allow_invalid_cert);
        } else {
            // http
            {
                std::lock_guard<std::mutex> guard(_lock);
                _shutdown_check_early();
                _socket = &_socket_container.emplace<1>(true);
            }
            _socket->connect(_hostname, _port);
        }

        // send request line
        _socket->send(_method + " " + _path + " HTTP/1.1" CRLF);

        // send headers, prioritizing some
        auto header_host = _rq_headers.find("host");
        if (header_host != _rq_headers.end()) {
            _socket->send(make_header_line(header_host->first, header_host->second));
            _rq_headers.erase(header_host);
        }

        auto header_user_agent = _rq_headers.find("user-agent");
        if (header_user_agent != _rq_headers.end()) {
            _socket->send(make_header_line(header_user_agent->first, header_user_agent->second));
            _rq_headers.erase(header_user_agent);
        }

        for (const auto &h : _rq_headers)
            _socket->send(make_header_line(h.first, h.second));

        // empty line to mark end of headers
        _socket->send(CRLF);
        _socket->flush();
        _state = STATE_RECEIVING_HEADERS;

        // receive response status-line
        std::string line;
        if (!_socket->recv_line(line, true, CRLF, STRTB_HTTP_STATUS_LINE_MAX_LEN)) {
            if (line.length() == STRTB_HTTP_STATUS_LINE_MAX_LEN)
                throw invalid_message("response status line too long");
            else
                throw invalid_message("incomplete response status line");
        }

        auto status_line = parse_status_line(line);
        if (!status_line.valid)
            throw invalid_message("invalid response status line");
        if (status_line.version.major != 1)
            throw invalid_message("incompatible response HTTP version");
        _status_code = status_line.status_code;
        _status_message = std::move(status_line.reason_phrase);
        _rs_http_version = status_line.version;

        // receive response headers
        while (true) {
            // TODO: obs-fold is allowed, give a param to enable/disable that
            // TODO: limit max amount of trailers to receive

            if (!_socket->recv_line(line, true, CRLF, STRTB_HTTP_FIELD_LINE_MAX_LEN)) {
                if (line.length() >= STRTB_HTTP_FIELD_LINE_MAX_LEN)
                    throw invalid_message("response header line too long");
                else
                    throw invalid_message("incomplete response header line");
            }

            // empty line marks end of headers
            if (line.empty())
                break;

            if (!_rs_headers.process_line(line))
                throw invalid_message("invalid response header line");
        }

        // TODO: Transfer-Encoding in HTTP/1.0 MUST be treated as faulty framing and close the connection afterwards

        // determine if a body is present
        if (_method == "HEAD" || _status_code == 204 || _status_code == 304 || _status_code / 100 == 1) {
            // certain methods and status codes cannot have a body
            _state = STATE_DONE;
            _content_length = 0;
            _content_length_known = true;
        } else {
            // check and parse content-encoding
            auto ce = _rs_headers.fields.find("content-encoding");
            if (ce != _rs_headers.fields.end()) {
                auto ce_parsed = parse_field_token_list(ce->second, false);
                if (!ce_parsed.valid)
                    throw invalid_message("invalid response content encoding");
                _content_encoding = std::move(ce_parsed.list);
            }

            auto te = _rs_headers.fields.find("transfer-encoding");
            if (te != _rs_headers.fields.end()) {
                _state = STATE_RECEIVING_BODY;
                _content_length_known = false;

                // parse transfer-encoding header
                auto te_parsed = parse_field_token_params_list(te->second, false, true);
                if (!te_parsed.valid)
                    throw invalid_message("invalid response transfer encoding");

                _transfer_encoding = std::move(te_parsed.list);
                // TODO: check return value to decide if the connection needs to close afterwards
                transfer_encoding_make_decoders(_decoders, _transfer_encoding, _rs_trailers, *_socket, true);
            } else {
                auto ce = _rs_headers.fields.find("content-length");

                if (ce != _rs_headers.fields.end()) {
                    // parse content-length header
                    auto ce_parsed = parse_field_integer(ce->second);
                    if (!ce_parsed.valid) {
                        if (ce_parsed.overflow)
                            throw unsupported_message("response content length is too long");
                        else
                            throw invalid_message("invalid response content length");
                    }

                    _content_length = ce_parsed.number;
                    _content_length_known = true;
                    _state = STATE_RECEIVING_BODY;
                    _decoders.push_back(std::unique_ptr<decoder>(new body_fixed_length(*_socket, _content_length)));
                } else {
                    // no encoding or length info
                    _state = STATE_RECEIVING_BODY;
                    _content_length = 0;
                    _content_length_known = false;
                    _decoders.push_back(std::unique_ptr<decoder>(new body_until_close(*_socket)));
                }
            }
        }

        // close connection if there's no body
        // TODO: remove this when persistent connections are implemented
        if (_state == STATE_DONE)
            _socket->close();

        // handle content encodings
        if (_state == STATE_RECEIVING_BODY)
            content_encoding_make_decoders(_decoders, _content_encoding);

        return _status_code;
    } catch (...) {
        clear();
        // check if the error was caused by a shutdown
        _shutdown_check();
        throw;
    }
}

std::pair<const char*, size_t> client::recv_body() {
    return recv_body(_socket->buffer_size());
    // TODO: determine what's an actual good default max_len
}

std::pair<const char*, size_t> client::recv_body(size_t max_len) {
    // TODO: handle transfer encodings

    if (_state != STATE_RECEIVING_BODY) {
        if (_state < STATE_RECEIVING_BODY)
            throw bad_state("request not prepared or sent");
        else    // already received the entire body
            return {0, 0};
    }

    try {
        assert(!_decoders.empty());
        if (!_decoders.empty()) {
            auto ret = _decoders.back()->read(max_len);

            if (ret.second == 0) {  // reading 0 bytes means we reached the end of the body
                // unless a shutdown truncated part of the body and somehow didn't cause an error
                _shutdown_check();
                _state = STATE_DONE;
                _socket->close();
                // TODO: attempt graceful shutdown over TLS
            }

            return ret;
        } else {
            return {0, 0};
        }
    } catch (...) {
        clear();
        // check if the error was caused by a shutdown
        _shutdown_check();
        throw;
    }
}

const std::string& client::log_name() const {
    return _log_name;
}

client::state_enum client::state() const {
    return _state;
}

const std::string& client::method() const {
    return _method;
}

const std::string& client::hostname() const {
    return _hostname;
}

int client::port() const {
    return _port;
}

const std::string& client::path() const {
    return _path;
}

bool client::encrypted() const {
    return _encrypted;
}

int client::status_code() const {
    return _status_code;
}

const std::string& client::status_message() const {
    return _status_message;
}

http_version client::response_http_version() const {
    return _rs_http_version;
}

const std::string& client::response_header(const std::string &name) const {
    return _rs_headers.get_field(name);
}

const std::string* client::response_header_or_null(const std::string &name) const {
    return _rs_headers.get_field_or_null(name);
}

const std::map<std::string, std::string>& client::response_headers() const {
    return _rs_headers.fields;
}

const std::vector<std::string>& client::response_cookies_raw() const {
    return _rs_headers.fields_set_cookie;
}

const std::string& client::response_trailer(const std::string &name) const {
    return _rs_trailers.get_field(name);
}

const std::string* client::response_trailer_or_null(const std::string &name) const {
    return _rs_trailers.get_field_or_null(name);
}

const std::map<std::string, std::string>& client::response_trailers() const {
    return _rs_trailers.fields;
}

std::pair<size_t, bool> client::content_length() const {
    if (_content_length_known)
        return {_content_length, true};
    else
        return {0, false};
}

const std::vector<token_params>& client::transfer_encoding() const {
    return _transfer_encoding;
}

const std::vector<std::string>& client::content_encoding() const {
    return _content_encoding;
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
    if (_d) {
        // TODO: cancel in-progress request
        delete _d;
        _d = nullptr;
    }
}

client::request::request(request &&other) {
    // WARNING: DO NOT run this while using the other request from another thread
    _d = other._d;
    other._d = nullptr;
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

client::request&& client::request::with_headers(const std::map<std::string, std::string> &headers) {
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

client::request&& client::request::with_headers(const std::vector< std::pair<std::string, std::string> > &headers) {
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

client::request&& client::request::with_headers(
        std::initializer_list< std::pair<std::string_view, std::string_view> > headers) {
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

}
