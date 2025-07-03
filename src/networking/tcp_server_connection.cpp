#include "tcp_server_connection.h"

using namespace strtb::networking;

tcp_server_connection::tcp_server_connection(strtb::common::deregistration_interface<class tcp_server_connection*> *parent, size_t recv_buffer_size) : tcp_socket(recv_buffer_size), _parent(parent) {}

tcp_server_connection::~tcp_server_connection() {
    if (_sock != -1 && _parent)
        _parent->deregister(this);
}

void tcp_server_connection::connect(int fd, std::string server_ip, int server_port, std::string remote_ip, int remote_port) {
    std::lock_guard<std::recursive_mutex> guard(_lock);
    _sock = fd;
    _server_ip = server_ip;
    _server_port = server_port;
    _remote_ip = remote_ip;
    _remote_port = remote_port;
}

void tcp_server_connection::close() {
    tcp_socket::close();
    if (_parent)
        _parent->deregister(this);
}

std::string tcp_server_connection::server_ip() {
    std::lock_guard<std::recursive_mutex> guard(_lock);
    return _server_ip;
}

std::string tcp_server_connection::remote_ip() {
    std::lock_guard<std::recursive_mutex> guard(_lock);
    return _remote_ip;
}

int tcp_server_connection::server_port() {
    std::lock_guard<std::recursive_mutex> guard(_lock);
    return _server_port;
}

int tcp_server_connection::remote_port() {
    std::lock_guard<std::recursive_mutex> guard(_lock);
    return _remote_port;
}
