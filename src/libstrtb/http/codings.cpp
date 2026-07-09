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

body_chunked::body_chunked(networking::tcp_socket &socket, field_parser &trailers)
    : _socket(socket), _trailers(trailers) {}

std::pair<const char*, size_t> body_chunked::read(size_t max_len) {
    if (_done)  // already read entire body
        return {0, 0};

    if (_bytes_remaining == 0) {
        std::string line;
        bool endl = false;

        // consume CRLF after chunk-data
        if (!_first) {
            endl = _socket.recv_line(line, false, "\r\n", 2);
            if (!endl)
                throw bad_response("received chunk with improper termination");
        }

        _first = false;

        // read another chunk header
        endl = _socket.recv_line(line, true, "\r\n", STRTB_HTTP_CHUNK_LINE_MAX_LEN);
        if (!endl) {
            if (line.length() >= STRTB_HTTP_CHUNK_LINE_MAX_LEN)
                throw bad_response("received chunk metadata line too long");
            else
                throw bad_response("incomplete received chunk metadata line");
        }

        // chunk length
        auto parsed_chunk_len = parse_integer_hex(line);
        if (!parsed_chunk_len.valid) {
            if (parsed_chunk_len.overflow)
                throw unsupported_response("received chunk length too long");
            else
                throw bad_response("invalid received chunk length");
        }
        _bytes_remaining = parsed_chunk_len.number;
        size_t pos = parsed_chunk_len.to;

        // chunk extensions (ignored)
        auto parsed_chunk_ext = parse_parameters(line, pos, line.length(), true);
        if (parsed_chunk_ext.to != line.length())   // must reach end of line
            throw bad_response("invalid received chunk extensions");

        // check for last chunk
        if (_bytes_remaining == 0) {
            // get trailers
            // TODO: limit max amount of trailers to receive
            while (true) {
                endl = _socket.recv_line(line, true, "\r\n", STRTB_HTTP_FIELD_LINE_MAX_LEN);
                if (!endl) {
                    if (line.length() >= STRTB_HTTP_FIELD_LINE_MAX_LEN)
                        throw bad_response("response trailer line too long");
                    else
                        throw bad_response("incomplete response trailer line");
                }

                if (line.empty()) {     // marks end of trailer section and body
                    _done = true;
                    return {0, 0};
                }

                _trailers.process_line(line);
            }
        }
    }

    // read from chunk
    auto ret = _socket.recv(std::min(_bytes_remaining, max_len));
    assert(ret.second <= _bytes_remaining);

    if (ret.second == 0)
        throw premature_end("incomplete body received");
    _bytes_remaining -= ret.second;

    return ret;
}

}