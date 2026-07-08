#include "client.h"
#include "strescape.h"

#include <assert.h>

#define CRLF "\r\n"

using namespace std::string_literals;

namespace strtb::http {

exception::exception(const char *str) : _what(str) {}
exception::exception(const std::string &str) : _what(str) {}
exception::exception(std::string &&str) : _what(str) {}

const char* exception::what() const noexcept {
    return _what.c_str();
}

client::client(const std::string &log_name) :
    _log_name(log_name),
    _log(log_name.empty() ? "HTTP Client" : "HTTP Client: " + log_name, false) {}

client& client::open(const std::string &method, const std::string &url, bool allow_invalid_cert, bool allow_unsafe_ports) {
    clear();

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
                throw security_precaution("test");
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
            set_header("host", host + ":" + std::to_string(_port));

        // set some headers
        set_header("user-agent", get_default_user_agent());
        set_header("connection", "close");
        set_header("accept-encoding", "identity");
    } catch (...) {
        clear();
        throw;
    }

    return *this;
}

client& client::set_header(const std::string &name, const std::string &value) {
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
    // TODO: move this into cancel() afterwards, checking if more steps are needed
    {
        std::lock_guard<std::mutex> guard(_lock);
        if (_socket) {
            if (_socket->is_open())
                _socket->close();
            _socket = nullptr;
            _socket_container = false;
        }
    }

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
    _rs_http_version.major = 0;
    _rs_http_version.minor = 0;
    _content_length = 0;
    _content_length_decoded = 0;
    _content_length_read = 0;
    _content_length_known = false;
    _content_length_decoded_known = false;
    _content_ends_on_close = false;
    _transfer_encoding.clear();
    _content_encoding.clear();
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
            networking::tcp_client_ssl &s = _socket_container.emplace<2>(true);
            _socket = &s;
            s.connect(_hostname, _port, true, !_allow_invalid_cert);
        } else {
            // http
            _socket = &_socket_container.emplace<1>(true);
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
        const size_t max_len = 16384;
        if (!_socket->recv_line(line, true, CRLF, max_len)) {
            if (line.length() == max_len)
                throw bad_response("response status line too long");
            else
                throw bad_response("incomplete response status line");
        }

        auto status_line = parse_status_line(line);
        if (!status_line.valid)
            throw bad_response("invalid response status line");
        if (status_line.version.major != 1)
            throw bad_response("incompatible response HTTP version");
        _status_code = status_line.status_code;
        _status_message = std::move(status_line.reason_phrase);
        _rs_http_version = status_line.version;

        // receive response headers
        while (true) {
            // TODO: obs-fold is allowed, give a param to enable/disable that

            if (!_socket->recv_line(line, true, CRLF, max_len)) {
                if (line.length() == max_len)
                    throw bad_response("response header line too long");
                else
                    throw bad_response("incpomplete response header line");
            }

            // empty line marks end of headers
            if (line.empty())
                break;

            if (!_rs_headers.process_line(line))
                throw bad_response("invalid response header line");
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
                    throw bad_response("invalid response content encoding");
                _content_encoding = std::move(ce_parsed.list);
            }

            auto te = _rs_headers.fields.find("transfer-encoding");
            if (te != _rs_headers.fields.end()) {
                _state = STATE_RECEIVING_BODY;
                _content_length_known = false;

                // parse transfer-encoding header
                auto te_parsed = parse_field_token_params_list(te->second, false, true);
                if (!te_parsed.valid)
                    throw bad_response("invalid response transfer encoding");

                // TODO: check for unsupported encodings and convert codings to either enums or conversion objects
                _transfer_encoding = std::move(te_parsed.list);
                // TODO: if the last coding ISN'T chunked, body end is marked by connection closing
                _content_ends_on_close = !(!_transfer_encoding.empty() && _transfer_encoding.back().token == "chunked");
            } else {
                auto ce = _rs_headers.fields.find("content-length");

                if (ce != _rs_headers.fields.end()) {
                    // parse content-length header
                    auto ce_parsed = parse_field_integer(ce->second);
                    if (!ce_parsed.valid) {
                        if (ce_parsed.overflow)
                            throw unsupported_response("response content length is too long");
                        else
                            throw bad_response("invalid response content length");
                    }

                    _content_length = ce_parsed.number;
                    _content_length_known = true;
                    _state = _content_length ? STATE_RECEIVING_BODY : STATE_DONE;
                } else {
                    // no encoding or length info
                    _state = STATE_RECEIVING_BODY;
                    _content_length = 0;
                    _content_length_known = false;
                    _content_ends_on_close = true;
                }
            }
        }

        // we only know the final decoded length prematurely if content-length is known and content is not encoded
        if (_content_length_known && _content_encoding.empty())
            _content_length_decoded_known = true;

        // close connection if there's no body
        // TODO: remove this when persistent connections are implemented
        if (_state == STATE_DONE)
            _socket->close();

        return _status_code;
    } catch (...) {
        clear();
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

    // only read upto the end of the content (if known)
    if (_content_length_known)
        max_len = std::min(_content_length - _content_length_read, max_len);

    auto ret = _socket->recv(max_len);
    _content_length_read += ret.second;
    _content_length_decoded = _content_length_read; // TODO: change this when decoding
    // check if socket returned more data than asked
    assert(!_content_length_known || _content_length >= _content_length_read);

    if (_content_length_known) {
        if (_content_length_read >= _content_length) {
            // reached end of body after we read enough bytes
            _state = STATE_DONE;
            _socket->close();
        } else if (ret.second == 0) {
            // prematurely reached end of body
            _state = STATE_DONE;
            _socket->close();
            throw premature_end("incomplete body received");
        }
    } else {    // content length not known
        // TODO: for HTTPS connections, treat abrupt ends as an incomplete body
        if (ret.second == 0) {
            // connection closure marks end of body
            _state = STATE_DONE;
            _socket->close();
        }
    }

    return ret;
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

const std::string& client::response_header_raw(const std::string &name) const {
    return _rs_headers.get_field(name);
}

const std::string* client::response_header_raw_or_null(const std::string &name) const {
    return _rs_headers.get_field_or_null(name);
}

const std::map<std::string, std::string>& client::response_headers_raw() const {
    return _rs_headers.fields;
}

const std::vector<std::string>& client::response_cookies_raw() const {
    return _rs_headers.fields_set_cookie;
}

std::pair<size_t, bool> client::content_length() const {
    if (_content_length_known)
        return {_content_length, true};
    else
        return {0, false};
}

std::pair<size_t, bool> client::content_length_decoded() const {
    if (_content_length_decoded_known)
        return {_content_length_decoded, true};
    else
        return {0, false};
}

const std::vector<token_params>& client::transfer_encoding() const {
    return _transfer_encoding;
}

const std::vector<std::string>& client::content_encoding() const {
    return _content_encoding;
}

}
