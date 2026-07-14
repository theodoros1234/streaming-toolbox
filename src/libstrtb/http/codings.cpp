#include "codings.h"
#include "protocol.h"
#include "../logging.h"
#include "../strescape.h"
#include <assert.h>
#include <cstring>
#include <string>

using namespace std::string_literals;

namespace strtb::http {

static logging::source log("HTTP Codings"s, false);

body_fixed_length::body_fixed_length(networking::tcp_socket &socket, size_t content_length)
    : _socket(socket), _bytes_remaining(content_length) {}

std::pair<const char*, size_t> body_fixed_length::read() {
    return read(_socket.buffer_size());
}

std::pair<const char*, size_t> body_fixed_length::read(size_t max_len) {
    if (_bytes_remaining == 0)  // message already fully read
        return {0, 0};

    // only read upto the amount of bytes remaining
    auto ret = _socket.recv(std::min(max_len, _bytes_remaining));
    assert(ret.second <= _bytes_remaining);

    if (ret.second == 0)
        throw incomplete_message("incomplete body received");
    _bytes_remaining -= ret.second;
    return ret;
}

body_until_close::body_until_close(networking::tcp_socket &socket) : _socket(socket) {}

std::pair<const char*, size_t> body_until_close::read() {
    return read(_socket.buffer_size());
}

std::pair<const char*, size_t> body_until_close::read(size_t max_len) {
    return _socket.recv(max_len);
}

body_chunked::body_chunked(networking::tcp_socket &socket, field_parser &trailers)
    : _socket(socket), _trailers(trailers) {}

std::pair<const char*, size_t> body_chunked::read() {
    return read(_socket.buffer_size());
}

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
                throw invalid_message("received chunk with improper termination");
        }

        _first = false;

        // read another chunk header
        endl = _socket.recv_line(line, true, "\r\n", STRTB_HTTP_CHUNK_LINE_MAX_LEN);
        if (!endl) {
            if (line.length() >= STRTB_HTTP_CHUNK_LINE_MAX_LEN)
                throw invalid_message("received chunk metadata line too long");
            else
                throw invalid_message("incomplete received chunk metadata line");
        }

        // chunk length
        auto parsed_chunk_len = parse_integer_hex(line);
        if (!parsed_chunk_len.valid) {
            if (parsed_chunk_len.overflow)
                throw unsupported_message("received chunk length too long");
            else
                throw invalid_message("invalid received chunk length");
        }
        _bytes_remaining = parsed_chunk_len.number;
        size_t pos = parsed_chunk_len.to;

        // chunk extensions (ignored)
        auto parsed_chunk_ext = parse_parameters(line, pos, line.length(), true);
        if (parsed_chunk_ext.to != line.length())   // must reach end of line
            throw invalid_message("invalid received chunk extensions");

        // check for last chunk
        if (_bytes_remaining == 0) {
            // get trailers
            // TODO: limit max amount of trailers to receive
            while (true) {
                endl = _socket.recv_line(line, true, "\r\n", STRTB_HTTP_FIELD_LINE_MAX_LEN);
                if (!endl) {
                    if (line.length() >= STRTB_HTTP_FIELD_LINE_MAX_LEN)
                        throw invalid_message("trailer line too long");
                    else
                        throw invalid_message("incomplete trailer line");
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
        throw incomplete_message("incomplete body received");
    _bytes_remaining -= ret.second;

    return ret;
}

decoder_zlib::decoder_zlib(decoder& read_from, bool gzip) : _read_from(read_from) {
    // initialize zlib struct
    std::memset(&_stream, 0, sizeof(z_stream));
    int ret = inflateInit2(&_stream, gzip ? 16 + 15 : 15);  // +16 for gzip decoding

    if (ret != Z_OK) {
        if (_stream.msg) {
            throw internal_error("failed to initialize zlib: "s + _stream.msg);
        } else switch (ret) {
        case Z_MEM_ERROR:
            throw internal_error("failed to initialize zlib: not enough memory");
        case Z_VERSION_ERROR:
            throw internal_error("failed to initialize zlib: incompatible library version");
        case Z_STREAM_ERROR:
            throw internal_error("failed to initialize zlib: invalid parameters");
        default:
            throw internal_error("failed to initialize zlib");
        }
    }

    _stream.next_in = Z_NULL;
    _stream.avail_in = 0;
    _stream.next_out = _buf;
    _stream.avail_out = sizeof(_buf);
}

decoder_zlib::~decoder_zlib() {
    int ret = inflateEnd(&_stream);
    if (ret != Z_OK)
        log.warning({"zlib infateEnd returned error code ", ret});
}

std::pair<const char*, size_t> decoder_zlib::read() {
    return read(sizeof(_buf));
}

std::pair<const char*, size_t> decoder_zlib::read(size_t max_len) {
    if (_buf_pos >= _buf_filled) {   // no ready data in buffer, must decompress some
        if (_done)  // full stream already read
            return {(char*) _buf, 0};

        while (true) {
            _buf_filled = sizeof(_buf) - _stream.avail_out;
            if (_buf_filled > 0) {
                // available decompressed data to return
                _buf_pos = 0;
                _stream.next_out = _buf;
                _stream.avail_out = sizeof(_buf);
                break;
            } else if (_done) { // just finished without outputting any extra data
                return {(char*) _buf, 0};
            } else if (_stream.avail_in == 0 && !_maybe_more_output) {
                // need to grab more input data
                std::tie((const char*&) _stream.next_in, _stream.avail_in) = _read_from.read();
                if (_stream.avail_in == 0)
                    throw incomplete_message("incomplete compressed data");
            }

            // process compressed data
            int ret = inflate(&_stream, Z_NO_FLUSH);
            switch (ret) {
            case Z_OK:
                break;

            case Z_NEED_DICT:
            case Z_DATA_ERROR:
                throw invalid_message("invalid or corrupted compressed data");

            case Z_MEM_ERROR:
                throw internal_error("out of memory");

            case Z_BUF_ERROR:
                if (_maybe_more_output) {
                    _maybe_more_output = false;
                    break;
                }
                [[fallthrough]];
            case Z_STREAM_ERROR:
            default:
                throw internal_error("internal error during decompression");

            case Z_STREAM_END:  // end of compressed data
                // make sure there's no more garbage data afterwards
                // NOTE: this is also required to finalize any other decoders under this one
                if (_stream.avail_in > 0 || _read_from.read().second > 0)
                    throw invalid_message("invalid or corrupted compressed data: "
                                          "garbage data present after compressed section");
                _done = true;
                break;
            }

            // might need to output more data before needing to read from input
            _maybe_more_output = _stream.avail_out == 0;
        }
    }

    // return data from output buffer
    size_t len = std::min(max_len, _buf_filled - _buf_pos);     // return at most max_len bytes
    const char *ptr = (char*) _buf + _buf_pos;
    _buf_pos += len;
    return {ptr, len};
}

decoder_brotli::decoder_brotli(decoder& read_from) : _read_from(read_from) {
    // initialize brotli decoder
    _state = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
    if (!_state)
        throw internal_error("failed to initialize brotli decoder");
}

decoder_brotli::~decoder_brotli() {
    BrotliDecoderDestroyInstance(_state);
}

std::pair<const char*, size_t> decoder_brotli::read() {
    return read(sizeof(_buf));
}

std::pair<const char*, size_t> decoder_brotli::read(size_t max_len) {
    if (_buf_pos >= _buf_filled) {  // need to decompress more data
        if (_done)  // full stream already read
            return {_buf, 0};

        while (true) {
            _buf_filled = sizeof(_buf) - _avail_out;
            if (_buf_filled > 0) {
                // available data to return
                _buf_pos = 0;
                _avail_out = sizeof(_buf);
                break;
            } else if (_done) {
                // just finished without outputting any data
                return {_buf, 0};
            } else if (_avail_in == 0 && _ret != BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
                // need to grab more input data
                std::tie(_next_in, _avail_in) = _read_from.read();
                if (_avail_in == 0)
                    throw incomplete_message("incomplete compressed data");
            }

            // process compressed data
            uint8_t *next_out = (uint8_t*) _buf;
            _ret = BrotliDecoderDecompressStream(_state,
                                                 &_avail_in, (const uint8_t**) &_next_in,
                                                 &_avail_out, &next_out, nullptr);

            switch (_ret) {
            case BROTLI_DECODER_RESULT_ERROR:
                throw internal_error("failed to decompress brotli data: "s +
                                     BrotliDecoderErrorString(BrotliDecoderGetErrorCode(_state)));

            case BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT:
            case BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT:
                break;

            case BROTLI_DECODER_RESULT_SUCCESS:     // end of compressed data
                // make sure there's no more garbage data afterwards
                // NOTE: this is also required to finalize any other decoders under this one
                if (_avail_in > 0 || _read_from.read().second > 0)
                    throw invalid_message("invalid or corrupted compressed data: "
                                          "garbage data present after compressed section");
                _done = true;
                break;
            }
        }
    }

    // return data from output buffer
    size_t len = std::min(max_len, _buf_filled - _buf_pos);     // return at most max_len bytes
    const char *ptr = _buf + _buf_pos;
    _buf_pos += len;
    return {ptr, len};
}

decoder_zstd::decoder_zstd(decoder& read_from) : _read_from(read_from) {
    // create and init zstd stream
    _stream = ZSTD_createDStream();
    if (!_stream)
        throw internal_error("failed to create zstd decoder");
    try {
        size_t init_ret = ZSTD_initDStream(_stream);
        if (ZSTD_isError(init_ret))
            throw internal_error("failed to initialize zstd decoder"s + ZSTD_getErrorName(init_ret));
    } catch (...) {
        ZSTD_freeDStream(_stream);
        throw;
    }

    // init buffers
    _zin = {nullptr, 0, 0};
    _zout = {_buf, sizeof(_buf), 0};
}

decoder_zstd::~decoder_zstd() {
    size_t ret = ZSTD_freeDStream(_stream);
    if (ZSTD_isError(ret))
        log.warning({"ZSTD_freeDStream returned an error: ", ZSTD_getErrorName(ret)});
}

std::pair<const char*, size_t> decoder_zstd::read() {
    return read(sizeof(_buf));
}

std::pair<const char*, size_t> decoder_zstd::read(size_t max_len) {
    if (_buf_pos >= _buf_filled) {  // need to decompress more data
        if (_done)  // full stream already read
            return {_buf, 0};

        while (true) {
            _buf_filled = _zout.pos;
            if (_zout.pos > 0) {
                // available data to return
                _buf_pos = 0;
                _zout.pos = 0;
                break;
            } else if (_done) {
                // just fiished without outputting any data
                return {_buf, 0};
            } else if (_zin.pos >= _zin.size && !_more_output) {
                // need to grab more input data
                _zin.pos = 0;
                std::tie(_zin.src, _zin.size) = _read_from.read();
                if (_zin.size == 0) {
                    if (_zret == 0) {
                        // done
                        _done = true;
                        return {_buf, 0};
                    } else {
                        // ended in the middle of a zstd frame
                        throw incomplete_message("incomplete compressed data");
                    }
                }
            }

            _more_output = false;
            _zret = ZSTD_decompressStream(_stream, &_zout, &_zin);

            if (ZSTD_isError(_zret)) {
                // NOTE: error codes require at least zstd v1.3.1 (released in 2017)
                switch (ZSTD_getErrorCode(_zret)) {
                case ZSTD_error_corruption_detected:
                case ZSTD_error_checksum_wrong:
                    throw invalid_message("invalid or corrupted compressed data: "s + ZSTD_getErrorName(_zret));

                default:
                    throw internal_error("failed to decompress zstd data: "s + ZSTD_getErrorName(_zret));

                case ZSTD_error_noForwardProgress_destFull:
                    _more_output = true;
                    break;
                }
            }
        }
    }

    // return data from output buffer
    size_t len = std::min(max_len, _buf_filled - _buf_pos);     // return at most max_len bytes
    const char *ptr = _buf + _buf_pos;
    _buf_pos += len;
    return {ptr, len};
}

void content_encoding_make_decoders(std::vector< std::unique_ptr<decoder> > &decoders,
                                    std::vector<std::string> &content_encoding,
                                    size_t max_decoders) {
    // there must be AT LEAST one decoder already (at least the one that interfaces with the socket)
    if (decoders.empty())
        throw internal_error("missing socket interface");

    // go through the content encodings backwards
    if (!content_encoding.empty()) {
        for (auto itr = content_encoding.end() - 1; itr >= content_encoding.begin(); itr--) {
            if (decoders.size() >= max_decoders)
                throw security_precaution("message uses too many encodings; blocking to prevent DoS");

            // TODO: faster string matching
            if (*itr == "gzip" || *itr == "x-gzip")
                decoders.push_back(std::unique_ptr<decoder>(new decoder_zlib(*decoders.back(), true)));
            else if (*itr == "deflate")
                decoders.push_back(std::unique_ptr<decoder>(new decoder_zlib(*decoders.back(), false)));
            else if (*itr == "br")
                decoders.push_back(std::unique_ptr<decoder>(new decoder_brotli(*decoders.back())));
            else if (*itr == "zstd")
                decoders.push_back(std::unique_ptr<decoder>(new decoder_zstd(*decoders.back())));
            else
                throw unsupported_message("unsupported content encoding " + string_escape(*itr));
        }
    }
}

}