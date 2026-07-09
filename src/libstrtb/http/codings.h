#ifndef STRTB_HTTP_CODINGS_H
#define STRTB_HTTP_CODINGS_H

#include <cstddef>
#include <utility>
#include "../networking/tcp_socket.h"
#include "protocol.h"

namespace strtb::http {

// TODO: fix potential problems on 32-bit systems mixing size_t and unsigned long long types

class decoder {
public:
    virtual ~decoder() = default;
    virtual std::pair<const char*, size_t> read(size_t max_len) = 0;
};

// used when the content length is known
class body_fixed_length : public decoder {
private:
    networking::tcp_socket &_socket;
    size_t _bytes_remaining = 0;

public:
    body_fixed_length(networking::tcp_socket &socket, size_t content_length);
    ~body_fixed_length() = default;
    std::pair<const char*, size_t> read(size_t max_len);
};

class body_until_close : public decoder {
private:
    networking::tcp_socket &_socket;

public:
    body_until_close(networking::tcp_socket &socket);
    ~body_until_close() = default;
    std::pair<const char*, size_t> read(size_t max_len);
};

class body_chunked : public decoder {
private:
    networking::tcp_socket &_socket;
    field_parser &_trailers;
    size_t _bytes_remaining = 0;    // for current chunk
    bool _done = false, _first = true;

public:
    body_chunked(networking::tcp_socket &socket, field_parser &trailers);
    ~body_chunked() = default;
    std::pair<const char*, size_t> read(size_t max_len);
};

}

#endif // STRTB_HTTP_CODINGS_H
