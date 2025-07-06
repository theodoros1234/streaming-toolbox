#include "tcp_server_connection_ssl.h"
#include "tcp_server_ssl.h"

using namespace strtb::networking;

tcp_server_ssl::tcp_server_ssl(SSL_CTX* ctx) {
    if (ctx) {
        // Caller-provided SSL context
        _ctx = ctx;
        _ctx_caller_provided = true;
    } else {
        // Default SSL context
        _ctx = SSL_CTX_new(TLS_server_method());
        if (!_ctx)
            throw internal_error_ssl("Failed to create default SSL context", 0);

        if (!SSL_CTX_set_min_proto_version(_ctx, TLS1_2_VERSION)) {
            SSL_CTX_free(_ctx);
            throw internal_error_ssl("Failed to set minimum TLS protocol version for default SSL context", 0);
        }

        uint64_t default_opts = SSL_OP_NO_RENEGOTIATION | SSL_OP_CIPHER_SERVER_PREFERENCE;
        SSL_CTX_set_options(_ctx, default_opts);
    }
}

tcp_server_ssl::~tcp_server_ssl() {
    if (!_ctx_caller_provided) {
        SSL_CTX_free(_ctx);
        _ctx = nullptr;
    }
}

tcp_server_connection* tcp_server_ssl::_new_connection(int sock, std::string remote_ip, int remote_port) {
    return new tcp_server_connection_ssl(this, _recv_buffer_size, sock, _server_ip, _server_port, remote_ip, remote_port, _ctx);
}

SSL_CTX* tcp_server_ssl::ssl_ctx() const {return _ctx;}

tcp_server_connection_ssl* tcp_server_ssl::accept() {
    return (tcp_server_connection_ssl*) tcp_server::accept();
}
