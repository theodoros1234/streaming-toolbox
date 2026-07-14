#ifndef STRTB_HTTP_CODINGS_H
#define STRTB_HTTP_CODINGS_H

#include <cstddef>
#include <memory>
#include <utility>
#include "../networking/tcp_socket.h"
#include "protocol.h"
#include <zlib.h>
#include <brotli/decode.h>
#include <zstd.h>

#define STRTB_HTTP_CODINGS_BUFFER_SIZE 65536
#define STRTB_HTTP_MAX_DECODERS 5

namespace strtb::http {

// TODO: fix potential problems on 32-bit systems mixing size_t and unsigned long long types

class decoder {
public:
    decoder() = default;
    decoder(const decoder&) = delete;
    virtual ~decoder() = default;
    virtual std::pair<const char*, size_t> read() = 0;
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
    std::pair<const char*, size_t> read();
    std::pair<const char*, size_t> read(size_t max_len);
};

class body_until_close : public decoder {
private:
    networking::tcp_socket &_socket;

public:
    body_until_close(networking::tcp_socket &socket);
    ~body_until_close() = default;
    std::pair<const char*, size_t> read();
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
    std::pair<const char*, size_t> read();
    std::pair<const char*, size_t> read(size_t max_len);
};

// used for deflate and gzip encodings
class decoder_zlib : public decoder {
private:
    decoder &_read_from;
    z_stream _stream;
    unsigned char _buf[STRTB_HTTP_CODINGS_BUFFER_SIZE];
    size_t _buf_pos = 0, _buf_filled = 0;
    bool _done = false, _maybe_more_output = false;

public:
    decoder_zlib(decoder& read_from, bool gzip);
    ~decoder_zlib();
    std::pair<const char*, size_t> read();
    std::pair<const char*, size_t> read(size_t max_len);
};

class decoder_brotli : public decoder {
private:
    decoder &_read_from;
    BrotliDecoderState *_state = nullptr;
    BrotliDecoderResult _ret = BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT;
    char _buf[STRTB_HTTP_CODINGS_BUFFER_SIZE];
    const char *_next_in = nullptr;
    size_t _buf_pos = 0, _buf_filled = 0, _avail_in = 0, _avail_out = sizeof(_buf);
    bool _done = false;

public:
    decoder_brotli(decoder& read_from);
    ~decoder_brotli();
    std::pair<const char*, size_t> read();
    std::pair<const char*, size_t> read(size_t max_len);
};

class decoder_zstd : public decoder {
private:
    decoder &_read_from;
    ZSTD_DStream *_stream = nullptr;
    ZSTD_inBuffer_s _zin;
    ZSTD_outBuffer_s _zout;
    char _buf[STRTB_HTTP_CODINGS_BUFFER_SIZE];
    size_t _buf_pos = 0, _buf_filled = 0, _zret = 0;
    bool _done = false, _more_output = false;

public:
    decoder_zstd(decoder& read_from);
    ~decoder_zstd();
    std::pair<const char*, size_t> read();
    std::pair<const char*, size_t> read(size_t max_len);
};

void content_encoding_make_decoders(std::vector< std::unique_ptr<decoder> > &decoders,
                                    std::vector<std::string> &content_encoding,
                                    size_t max_decoders = STRTB_HTTP_MAX_DECODERS);

bool transfer_encoding_make_decoders(std::vector< std::unique_ptr<decoder> > &decoders,
                                     std::vector<token_params> &transfer_encoding,
                                     field_parser &trailers,
                                     networking::tcp_socket &socket,
                                     bool allow_no_chunked,
                                     size_t max_decoders = STRTB_HTTP_MAX_DECODERS);

}

#endif // STRTB_HTTP_CODINGS_H
