#include "client.h"

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
        switch (_url.host_type) {
        case uri::HOST_REGNAME:
        case uri::HOST_IPV4:
            _hostname = _url.host_str(true);
            break;

        case uri::HOST_IPV6:
            _hostname = _url.host_str(true).substr(1, _url.host_to - _url.host_from - 2);   // trim square brackets
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
    } catch (...) {
        clear();
        throw;
    }

    return *this;
}

}
