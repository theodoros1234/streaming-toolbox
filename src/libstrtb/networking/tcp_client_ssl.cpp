#include "tcp_client_ssl.h"
#include "../logging/logging.h"
#include "sigpipe_suppressor.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <sys/socket.h>
#include <assert.h>
#include <poll.h>

using namespace strtb;
using namespace strtb::networking;

static logging::source log("TCP Client with SSL/TLS", false);

// Default context wrapped in a struct so that it gets automatically created and freed on program load/unload
static struct default_context_container {
    SSL_CTX* ctx = nullptr;
    default_context_container() {
        ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) {
            log.put(logging::ERROR, {"Failed to create the default SSL context"});
            return;
        }

        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

        if (!SSL_CTX_set_default_verify_paths(ctx)) {
            log.put(logging::ERROR, {"Failed to set the default trusted certificate store for the default SSL context"});
            SSL_CTX_free(ctx);
            return;
        }

        if (!SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION)) {
            log.put(logging::ERROR, {"Failed to set the minimum TLS protocol version for the default SSL context"});
            SSL_CTX_free(ctx);
            return;
        }
    }

    ~default_context_container() {
        if (ctx)
            SSL_CTX_free(ctx);
        ctx = nullptr;
    }
} default_context;

tcp_client_ssl::tcp_client_ssl() : tcp_client(STRTB_NETWORKING_RECV_BUFFER_SIZE_DEFAULT_SSL) {}

tcp_client_ssl::tcp_client_ssl(size_t recv_buffer_size) : tcp_client(recv_buffer_size) {}

tcp_client_ssl::~tcp_client_ssl() {
    if (_ssl) {
        log.put(logging::WARNING, {"Destructor called when socket was still open. Closing the SSL connection and the socket, but this may lead to a crash. If you're a plugin developer, make sure you call close() on the socket."});
        ::shutdown(_sock, SHUT_RDWR);
        _thread.stop();
        SSL_free(_ssl);
        _ssl = nullptr;
        _sock = -1;
    }
}

void tcp_client_ssl::connect(const char* address, uint16_t port, time_t timeout) {
    connect(address, port, false, true, nullptr, timeout);
}

void tcp_client_ssl::connect(const std::string& address, uint16_t port, bool allow_abrupt_shutdown, bool verify_certificate, SSL_CTX* ssl_context, time_t timeout) {
    connect(address.c_str(), port, allow_abrupt_shutdown, verify_certificate, ssl_context, timeout);
}

void tcp_client_ssl::connect(const char* address, uint16_t port, bool allow_abrupt_shutdown, bool verify_certificate, SSL_CTX* ssl_context, time_t timeout) {
    // TODO: think about locking the socket _lock cause it will prevent shutdown from being run, but also think about setting _sock to -1

    tcp_client::connect(address, port, timeout);

    if (!ssl_context) {
        if (!default_context.ctx)
            throw internal_error_ssl("Failed to get the default SSL context", 0);
        ssl_context = default_context.ctx;
    }

    _ssl = SSL_new(ssl_context);
    if (!_ssl) {
        tcp_client::close();
        throw internal_error_ssl("failed to create SSL object", 0);
    }

    if (verify_certificate)
        SSL_set_verify(_ssl, SSL_VERIFY_PEER, NULL);
    else
        SSL_set_verify(_ssl, SSL_VERIFY_NONE, NULL);

    if (allow_abrupt_shutdown)
        SSL_set_options(_ssl, SSL_OP_IGNORE_UNEXPECTED_EOF);

    sigpipe_suppressor sp;
    BIO* bio = BIO_new(BIO_s_socket());
    if (!bio) {
        std::lock_guard<std::recursive_mutex> guard(_lock);
        tcp_client::close();
        SSL_free(_ssl);
        _ssl = nullptr;
        throw internal_error_ssl("failed to create BIO object for SSL", 0);
    }

    if (BIO_set_fd(bio, _sock, BIO_CLOSE) <= 0) {
        std::lock_guard<std::recursive_mutex> guard(_lock);
        tcp_client::close();
        SSL_free(_ssl);
        _ssl = nullptr;
        BIO_free(bio);
        throw internal_error_ssl("failed to associate underlying socket with BIO object for SSL", 0);
    }

    SSL_set_bio(_ssl, bio, bio);

    if (!SSL_set_tlsext_host_name(_ssl, address)) {
        std::lock_guard<std::recursive_mutex> guard(_lock);
        _sock = -1;
        SSL_free(_ssl);
        _ssl = nullptr;
        throw internal_error_ssl("failed to set the SNI hostname for SSL object", 0);
    }

    if (!SSL_set1_host(_ssl, address)) {
        std::lock_guard<std::recursive_mutex> guard(_lock);
        _sock = -1;
        SSL_free(_ssl);
        _ssl = nullptr;
        throw internal_error_ssl("failed to set the certificate verification hostname for SSL object", 0);
    }

    int ssl_connect_return = SSL_connect(_ssl);
    if (ssl_connect_return < 1) {
        long ssl_verify_result = SSL_get_verify_result(_ssl);
        int ssl_errno = SSL_get_error(_ssl, ssl_connect_return);

        std::lock_guard<std::recursive_mutex> guard(_lock);
        _sock = -1;
        SSL_free(_ssl);
        _ssl = nullptr;

        if (ssl_verify_result != X509_V_OK)
            throw ssl_verification_error(X509_verify_cert_error_string(ssl_verify_result), ssl_verify_result);
        else
            throw connection_error_ssl("SSL connection process failed", ssl_errno);
    }

    try {
        _thread.start(_sock, _ssl);
    } catch (...) {
        std::lock_guard<std::recursive_mutex> guard(_lock);
        _sock = -1;
        SSL_free(_ssl);
        _ssl = nullptr;
    }
}

ssize_t tcp_client_ssl::recv(size_t max_len) {
    assert((_sock == -1) == (_ssl == nullptr));

    if (_ssl == nullptr)
        throw connection_closed("socket closed or hasn't been opened yet", 0);

    if (max_len == 0)
        throw std::invalid_argument("max_len cannot be 0");

    if (max_len > _buffer_size)
        max_len = _buffer_size;

    // Return from recv_line's leftovers if there are enough to cover the request
    if (max_len <= _line_leftovers) {   // TODO: I haven't properly tested this part, but it should be working
        memmove(_buffer, _buffer + _line_leftovers_pos, max_len);
        _line_leftovers_pos += max_len;
        _line_leftovers -= max_len;
        return max_len;
    }

    // Move back any leftover bytes from recv_line
    if (_line_leftovers) {
        memmove(_buffer, _buffer + _line_leftovers_pos, _line_leftovers);
        _line_leftovers = 0;
    }

    return _thread.recv(_buffer + _line_leftovers, max_len - _line_leftovers);
}

ssize_t tcp_client_ssl::send(const char* buf, size_t len) {
    assert((_sock == -1) == (_ssl == nullptr));

    if (_ssl == nullptr)
        throw connection_closed("socket closed or hasn't been opened yet", 0);

    return _thread.send(buf, len);
}

void tcp_client_ssl::shutdown_gracefully() {
    // NOTE: Only call this from the sender thread. For unexpectedly cancelling the connection, use shutdown()
    assert((_sock == -1) == (_ssl == nullptr));

    if (_ssl == nullptr)
        throw connection_closed("socket closed or hasn't been opened yet", 0);

    _thread.shutdown_gracefully();
}

void tcp_client_ssl::close() {
    std::lock_guard<std::recursive_mutex> guard(_lock);
    assert((_sock == -1) == (_ssl == nullptr));

    if (_ssl == nullptr)
        throw connection_closed("socket closed or hasn't been opened yet", 0);

    cancel_connect();
    shutdown(true, true);
    _remote_ip = "";
    _remote_port = 0;
    _sock = -1;
    _thread.stop();
    SSL_free(_ssl);
    _ssl = nullptr;
    buffer_clear();
}

SSL* tcp_client_ssl::ssl() const {
    return _ssl;
}
