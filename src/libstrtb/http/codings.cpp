#include "codings.h"
#include "protocol.h"
#include <assert.h>

namespace strtb::http {

body_fixed_length::body_fixed_length(networking::tcp_socket &socket, size_t content_length)
    : _socket(socket), _bytes_remaining(content_length) {}

std::pair<const char*, size_t> body_fixed_length::read(size_t max_len) {
    if (_bytes_remaining == 0)  // message already fully read
        return {0, 0};

    // only read upto the amount of bytes remaining
    auto ret = _socket.recv(std::min(max_len, _bytes_remaining));
    assert(ret.second <= _bytes_remaining);

    if (ret.second == 0)
        throw premature_end("incomplete body received");
    _bytes_remaining -= ret.second;
    return ret;
}

body_until_close::body_until_close(networking::tcp_socket &socket) : _socket(socket) {}

std::pair<const char*, size_t> body_until_close::read(size_t max_len) {
    return _socket.recv(max_len);
}

}