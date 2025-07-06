#include "tcp_server_connection.h"

using namespace strtb::networking;

tcp_server_connection::tcp_server_connection(strtb::common::deregistration_interface<class tcp_server_connection*> *parent,
                                             size_t recv_buffer_size,
                                             int fd,
                                             std::string server_ip,
                                             int server_port,
                                             std::string remote_ip,
                                             int remote_port) :
    tcp_socket(recv_buffer_size),
    _parent(parent),
    _server_ip(server_ip),
    _remote_ip(remote_ip),
    _server_port(server_port),
    _remote_port(remote_port) {
    _sock = fd;
}

tcp_server_connection::~tcp_server_connection() {
    if (_sock != -1 && _parent)
        _parent->deregister(this);
}

void tcp_server_connection::close() {
    tcp_socket::close();
    if (_parent)
        _parent->deregister(this);
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
