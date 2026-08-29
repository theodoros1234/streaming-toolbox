#include "client.h"
#include "../http/protocol.h"
#include "../uri.h"
#include "../strescape.h"
#include "../base64.h"
#include "../logging.h"

#include <stdexcept>
#include <set>
#include <random>
#include <mutex>
#include <openssl/evp.h>

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace strtb::websocket {

static std::mutex nonce_gen_lock;
static std::mt19937_64 nonce_gen = std::mt19937_64(std::random_device()());

client::handshake_failed::handshake_failed(const char *str, int status) :
    exception(str), _status(status) {}

client::handshake_failed::handshake_failed(const std::string &str, int status) :
    exception(str), _status(status) {}

client::handshake_failed::handshake_failed(std::string &&str, int status) :
    exception(str), _status(status) {}

client::handshake_failed::handshake_failed(std::string_view str, int status) :
    exception(str), _status(status) {}

int client::handshake_failed::status() const noexcept {
    return _status;
}

client::client() : _handshake_rq("GET"sv) {}

client::~client() {}

client& client::with_url(std::string_view url) {
    _valid_state(false);
    uri::parser parser;

    // parse URL
    auto [url_to, url_valid] = parser.parse_uri(url, false);
    if (!url_valid || parser.path_type != uri::PATH_ABEMPTY)
        throw std::invalid_argument("invalid or incompatible URL "
                                    "(note that IPv6 addresses must be enclosed in square brackets)");

    // check scheme
    std::string scheme = parser.scheme_str(url, true);
    bool secure = false;
    if (scheme == "wss" || scheme == "https")
        secure = true;
    else if (scheme == "ws" || scheme == "http")
        secure = false;
    else
        throw std::invalid_argument("incompatible URL scheme, only ws/wss/http/https allowed");

    // don't allow fragments
    if (parser.fragment_to)
        throw std::invalid_argument("URL fragments not allowed");

    // setup handshake request with specified or default port
    if (parser.port_from == parser.port_to)     // default
        _handshake_rq.with_host(secure, parser.host_str(url));
    else                                        // specified
        _handshake_rq.with_host(secure, parser.host_str(url), parser.port_uint16(url));

    // path (and query)
    if (parser.path_to)         // has a path (and maybe a query)
        _handshake_rq.with_path(url.substr(parser.path_from));
    else if (parser.query_to)   // only has a query
        _handshake_rq.with_path(url.substr(parser.query_from));
    else                        // has neither, use default
        _handshake_rq.with_path("/"sv);

    return *this;
}

client& client::with_host(bool secure, std::string_view hostname) {
    _valid_state(false);
    _handshake_rq.with_host(secure, hostname);
    return *this;
}

client& client::with_host(bool secure, std::string_view hostname, unsigned int port) {
    _valid_state(false);
    _handshake_rq.with_host(secure, hostname, port);
    return *this;
}

client& client::with_path(std::string_view path) {
    _valid_state(false);
    _handshake_rq.with_path(path);
    return *this;
}

client& client::with_params(const std::map<std::string, std::string> &params) {
    _valid_state(false);
    _handshake_rq.with_params(params);
    return *this;
}

client& client::with_params(const std::vector< std::pair<std::string, std::string> > &params) {
    _valid_state(false);
    _handshake_rq.with_params(params);
    return *this;
}

client& client::with_params(std::initializer_list< std::pair<std::string_view, std::string_view> > params) {
    _valid_state(false);
    _handshake_rq.with_params(params);
    return *this;
}

client& client::allow_invalid_cert(bool value) {
    _valid_state(false);
    _handshake_rq.allow_invalid_cert(value);
    return *this;
}

client& client::allow_unsafe_ports(bool value) {
    _valid_state(false);
    _handshake_rq.allow_unsafe_ports(value);
    return *this;
}

template<class T> client& client::_with_subprotocols(T list) {
    _valid_state(false);
    _subprotocols_wanted.clear();
    _subprotocols_wanted.reserve(list.size());

    std::set<std::string_view> unique;
    try {
        for (const auto &value : list) {
            // check syntax
            auto [valid, len] = http::parse_token(value);
            if (!valid && len != value.length())
                throw std::invalid_argument("invalid subprotocol name " + string_escape(value));

            // check for duplicates
            auto [itr, created] = unique.insert(value);
            if (!created)
                throw std::invalid_argument("duplicate subprotocol " + string_escape(value));

            // add to wanted list
            _subprotocols_wanted.emplace_back(value);
        }
    } catch (...) {
        _subprotocols_wanted.clear();
        throw;
    }

    return *this;
}

client& client::with_subprotocols(const std::vector<std::string> &list) {
    return _with_subprotocols<const std::vector<std::string> &>(list);
}

client& client::with_subprotocols(const std::vector<std::string_view> &list) {
    return _with_subprotocols<const std::vector<std::string_view> &>(list);
}

client& client::with_subprotocols(std::initializer_list<std::string_view> list) {
    return _with_subprotocols< std::initializer_list<std::string_view> >(list);
}

client& client::with_header(std::string_view name, const char *value) {
    _valid_state(false);
    _handshake_rq.with_header(name, value);
    return *this;
}

client& client::with_header(std::string_view name, std::string &&value) {
    _valid_state(false);
    _handshake_rq.with_header(name, std::move(value));
    return *this;
}

client& client::with_header(std::string_view name, std::string_view value) {
    _valid_state(false);
    _handshake_rq.with_header(name, value);
    return *this;
}

client& client::with_auth_bearer(std::string_view token) {
    _valid_state(false);
    _handshake_rq.with_auth_bearer(token);
    return *this;
}

client& client::with_auth_basic(std::string_view username, std::string_view password) {
    _valid_state(false);
    _handshake_rq.with_auth_basic(username, password);
    return *this;
}

client& client::with_auth_basic(std::string_view userinfo) {
    _valid_state(false);
    _handshake_rq.with_auth_basic(userinfo);
    return *this;
}

client& client::with_headers(const std::map<std::string, std::string> &headers) {
    _valid_state(false);
    _handshake_rq.with_headers(headers);
    return *this;
}

client& client::with_headers(const std::vector< std::pair<std::string, std::string> > &headers) {
    _valid_state(false);
    _handshake_rq.with_headers(headers);
    return *this;
}

client& client::with_headers(std::initializer_list< std::pair<std::string_view, std::string_view> > headers) {
    _valid_state(false);
    _handshake_rq.with_headers(headers);
    return *this;
}

client& client::with_headers(std::map<std::string, std::string> &&headers) {
    _valid_state(false);
    _handshake_rq.with_headers(std::move(headers));
    return *this;
}

client& client::with_headers(std::vector< std::pair<std::string, std::string> > &&headers) {
    _valid_state(false);
    _handshake_rq.with_headers(std::move(headers));
    return *this;
}

void client::clear_header(std::string_view name) {
    _valid_state(false);
    _handshake_rq.clear_header(name);
}

void client::clear_headers(std::initializer_list<std::string_view> list) {
    _valid_state(false);
    _handshake_rq.clear_headers(list);
}

void client::clear_headers() {
    _valid_state(false);
    _handshake_rq.clear_headers();
}

void client::clear() {
    _handshake_rq = http::get();
    _subprotocols_wanted.clear();
    _subprotocol_used.clear();
    _socket_container.emplace<std::monostate>();
    _socket = nullptr;
}

void client::_valid_state(bool connected) const {
    if ((_socket != nullptr) != connected) {
        if (connected)
            throw http::bad_state("websocket not connected");
        else
            throw http::bad_state("websocket already connected");
    }
}

void client::connect() {
    _valid_state(false);

    // generate key/nonce (16 bytes)
    constexpr size_t key_size_bytes = 16;
    constexpr size_t key_size = key_size_bytes / sizeof(uint64_t);
    uint64_t key[key_size];
    {
        std::lock_guard<std::mutex> lock_n(nonce_gen_lock);
        for (size_t i = 0; i < key_size; i++)
            key[i] = nonce_gen();
    }
    std::string key_base64 = base64_encode(std::string_view((char*) key, key_size_bytes));

    // subprotocols
    if (_subprotocols_wanted.empty())
        _handshake_rq.clear_header("sec-websocket-protocol"sv);
    else
        _handshake_rq.with_header("sec-websocket-protocol"sv, http::list_to_string(_subprotocols_wanted));

    // send HTTP request
    auto rs = _handshake_rq
                  .upgrade("websocket"sv)
                  .with_header("sec-websocket-version"sv, "13"sv)
                  .with_header("sec-websocket-key"sv, key_base64)
                  .send();

    // server must respond by upgrading the connection
    if (rs.status() != 101)
        throw handshake_failed("websocket handshake failed: server responded with " +
                                std::to_string(rs.status()) + " " + rs.status_message(), rs.status());

    // generate key expected to be received from server
    std::string key_concat = key_base64 + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    unsigned char key_concat_sha1[EVP_MAX_MD_SIZE];
    unsigned int key_concat_sha1_len = 0;
    // TODO: can this be optimized by using a context?
    if (!EVP_Digest(key_concat.data(), key_concat.length(),
                    key_concat_sha1, &key_concat_sha1_len, EVP_sha1(), NULL))
        throw http::internal_error("failed to generate the key's SHA1 digest");
    std::string key_concat_sha1_base64 = base64_encode(
        std::string_view((const char*) key_concat_sha1, key_concat_sha1_len));

    // check if the server returned the correct key
    try {
        if (rs.header("sec-websocket-accept"sv) != key_concat_sha1_base64)
            throw handshake_failed("websocket handshake failed: returned key is incorrect", rs.status());
    } catch (std::out_of_range&) {
        throw handshake_failed("websocket handshake failed: missing header "
                               "\"Sec-WebSocket-Accept\" from server response", rs.status());
    }

    // no extensions supported currently
    // TODO: after implementing extensions, properly check this header
    if (rs.header_or_null("sec-websocket-extensions"sv))
        throw handshake_failed("websocket extensions are not supported", rs.status());

    // check returned subprotocol
    const std::string *subprotocol = rs.header_or_null("sec-websocket-protocol"sv);
    if (subprotocol) {
        auto [subprotocol_valid, subprotocol_len] = http::parse_token(*subprotocol);
        if (!subprotocol_valid || subprotocol_len != subprotocol->length())
            throw handshake_failed("websocket handshake failed: server responded "
                                   "with an invalid \"Sec-WebSocket-Protocol\" header", rs.status());

        if (*subprotocol != "null") {
            bool subprotocol_wanted = false;
            for (const auto &item : _subprotocols_wanted) {
                if (item == *subprotocol) {
                    subprotocol_wanted = true;
                    break;
                }
            }

            if (!subprotocol_wanted)
                throw handshake_failed("websocket handshake failed: server responded "
                                       "with unwanted subprotocol " + string_escape(*subprotocol), rs.status());
        }

        _subprotocol_used = *subprotocol;
    }

    // grab socket from response
    std::lock_guard<std::mutex> lock(_mutex);
    try {
        _socket_container = rs.socket();
        switch (_socket_container.index()) {
        case 1:
            _socket = &std::get<1>(_socket_container);
            break;

        case 2:
            _socket = &std::get<2>(_socket_container);
            break;

        default:
            throw http::internal_error("unexpected socket type received "
                                       "(type " + std::to_string(_socket_container.index()) + ")");
        }
    } catch (...) {
        // TODO: only clear connection-related parts instead of everything, and mind the lock
        clear();
        throw;
    }
}

const std::string& client::subprotocol_used() const {
    // if empty, then no subprotocol is being used
    return _subprotocol_used;
}

// TODO: remove
void client::test_recv() {
    logging::source l("WebSocket recv test", false);
    const char *data = nullptr;
    size_t len = 0;

    frame_send(*_socket, OPCODE_TEXT, true, true, "Sending some test data from the client"sv);

    while (true) {
        if (len == 0)
            std::tie(data, len) = _socket->recv();

        if (len == 0)
            return;

        auto [bytes_read, frame_opt] = _frame_parser.process(data, 1, false, 10000000);
        data += bytes_read;
        len -= bytes_read;

        if (frame_opt) {
            auto frame = std::move(frame_opt.value());
            const char* opcode;
            switch (frame.opcode) {
            case OPCODE_CONT:
                opcode = "CONT";
                break;
            case OPCODE_TEXT:
                opcode = "TEXT";
                break;
            case OPCODE_BIN:
                opcode = "BIN";
                break;
            case OPCODE_RSV3:
                opcode = "RSV3";
                break;
            case OPCODE_RSV4:
                opcode = "RSV4";
                break;
            case OPCODE_RSV5:
                opcode = "RSV5";
                break;
            case OPCODE_RSV6:
                opcode = "RSV6";
                break;
            case OPCODE_RSV7:
                opcode = "RSV7";
                break;
            case OPCODE_CLOSE:
                opcode = "CLOSE";
                break;
            case OPCODE_PING:
                opcode = "PING";
                frame_send(*_socket, OPCODE_PONG, true, true, frame.payload);
                break;
            case OPCODE_PONG:
                opcode = "PONG";
                break;
            case OPCODE_RSVB:
                opcode = "RSVB";
                break;
            case OPCODE_RSVC:
                opcode = "RSVC";
                break;
            case OPCODE_RSVD:
                opcode = "RSVD";
                break;
            case OPCODE_RSVE:
                opcode = "RSVE";
                break;
            case OPCODE_RSVF:
                opcode = "RSVF";
                break;
            default:
                opcode = "INVALID";
            }

            l.info({"fin=", frame.fin, ", rsv1=", frame.rsv1, ", rsv2=", frame.rsv2, ", rsv3=", frame.rsv3,
                    ", opcode=", opcode, ", length=", frame.length, ", payload=", string_escape(frame.payload)});
        }
    }
}

}