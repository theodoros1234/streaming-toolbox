#include "tcp_server_connection.h"
#include "../logging.h"

using namespace strtb::networking;

static strtb::logging::source log("TCP Server Connection", false);

tcp_server_connection::tcp_server_connection(strtb::common::deregistration_interface<class tcp_server_connection*> *parent,
                                             bool buffered_send,
                                             size_t buffer_size,
                                             int fd,
                                             std::string server_ip,
                                             int server_port,
                                             std::string remote_ip,
                                             int remote_port) :
    tcp_socket(buffered_send, buffer_size),
    _parent(parent),
    _server_ip(server_ip),
    _remote_ip(remote_ip),
    _server_port(server_port),
    _remote_port(remote_port) {
    _prepare_buffers();
    _sock = fd;
}

tcp_server_connection::~tcp_server_connection() {
    try {
        // close socket if it's still open
        // NOTE: subclasses that override close() MUST handle this on their own destructors
        if (is_open())
            tcp_server_connection::close();
    } catch (std::exception &e) {
        log.critical({"Failed to destroy object: ", e.what()});
        std::terminate();
    }
}

void tcp_server_connection::close() {
    if (_parent) {
        _parent->deregister(this);
        _parent = nullptr;
    }
    tcp_socket::close();
}

const std::string& tcp_server_connection::server_ip() const {
    return _server_ip;
}

const std::string& tcp_server_connection::remote_ip() const {
    return _remote_ip;
}

int tcp_server_connection::server_port() const {
    return _server_port;
}

int tcp_server_connection::remote_port() const {
    return _remote_port;
}
