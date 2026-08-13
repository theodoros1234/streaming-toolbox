#include "tcp_client_ssl.h"
#include "tcp_socket_ssl_common.h"
#include "../logging.h"
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

tcp_client_ssl::tcp_client_ssl(bool buffered_send, bool thread_assisted, size_t recv_buffer_size)
    : tcp_client(buffered_send, recv_buffer_size),
    _thread_assisted(thread_assisted) {}

tcp_client_ssl::~tcp_client_ssl() {
    detach_shutdown_controller();

    if (_ssl) {
        log.put(logging::WARNING, {"Destructor called when socket was still open. Closing the SSL connection and the socket, but this may lead to a crash. If you're a plugin developer, make sure you call close() on the socket."});
        ::shutdown(_sock, SHUT_RDWR);
        if (_thread_assisted)
            _thread.stop();
        SSL_free(_ssl);
        _ssl = nullptr;
        _sock = -1;
    }
}

void tcp_client_ssl::connect(const std::string& address, uint16_t port, bool allow_abrupt_shutdown, bool verify_certificate, SSL_CTX* ssl_context, time_t timeout) {
    connect(address.c_str(), port, allow_abrupt_shutdown, verify_certificate, ssl_context, timeout);
}

void tcp_client_ssl::connect(const char* address, uint16_t port, bool allow_abrupt_shutdown, bool verify_certificate, SSL_CTX* ssl_context, time_t timeout) {
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
        if (_thread_assisted)
            _thread.start(_sock, _ssl);
    } catch (...) {
        std::lock_guard<std::recursive_mutex> guard(_lock);
        _sock = -1;
        SSL_free(_ssl);
        _ssl = nullptr;
    }
}

size_t tcp_client_ssl::_recv(size_t len) {
    assert((_sock == -1) == (_ssl == nullptr));
    if (_thread_assisted)
        return _thread.recv(_buffer_recv, len);
    else
        return _ssl_recv(_ssl, _buffer_recv, len);
}

void tcp_client_ssl::_send(const char* buf, size_t len) {
    assert((_sock == -1) == (_ssl == nullptr));
    if (_thread_assisted)
        _thread.send(buf, len);
    else
        return _ssl_send(_ssl, buf, len);
}

void tcp_client_ssl::shutdown_gracefully() {
    // NOTE: Only call this from the sender thread. For unexpectedly cancelling the connection, use shutdown()
    assert((_sock == -1) == (_ssl == nullptr));

    if (_ssl == nullptr)
        throw connection_closed("socket closed or hasn't been opened yet", 0);

    if (_thread_assisted)
        _thread.shutdown_gracefully();
    else
        _ssl_shutdown_gracefully(_ssl);
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
    if (_thread_assisted)
        _thread.stop();
    SSL_free(_ssl);
    _ssl = nullptr;
    _line_leftovers = 0;
}

SSL* tcp_client_ssl::ssl() const {
    return _ssl;
}

bool tcp_client_ssl::thread_assisted() const {
    return _thread_assisted;
}

bool tcp_client_ssl::_available() {
    assert((_sock == -1) == (_ssl == nullptr));
    if (_thread_assisted)
        return _thread.available();
    else
        return _ssl_available(_ssl, _sock);
}

void tcp_client_ssl::thread_assist_enable() {
    if (_connecting)
        throw std::logic_error("cannot enable thread assistance while connecting");

    if (!_thread_assisted) {
        _thread_assisted = true;
        if (is_open())
            _thread.start(_sock, _ssl);
    }
}

void tcp_client_ssl::thread_assist_disable() {
    if (_connecting)
        throw std::logic_error("cannot disable thread assistance while connecting");

    if (_thread_assisted) {
        _thread_assisted = false;
        if (is_open())
            _thread.stop();
        _thread.release();
    }
}
