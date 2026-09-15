#include "protocol.h"
#include "../networking/tcp_client_ssl.h"
#include "../networking/tcp_server_connection_ssl.h"

#include <cassert>
#include <openssl/rand.h>
#include <openssl/err.h>

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace strtb::websocket {

handshake_failed::handshake_failed(const char *str, int status) :
    exception(str), _status(status) {}

handshake_failed::handshake_failed(const std::string &str, int status) :
    exception(str), _status(status) {}

handshake_failed::handshake_failed(std::string &&str, int status) :
    exception(std::move(str)), _status(status) {}

handshake_failed::handshake_failed(std::string_view str, int status) :
    exception(str), _status(status) {}

int handshake_failed::status() const noexcept {
    return _status;
}

invalid_frame::invalid_frame(const char *str, int error_code) :
    exception(str), _error_code(error_code) {}

invalid_frame::invalid_frame(const std::string &str, int error_code) :
    exception(str), _error_code(error_code) {}

invalid_frame::invalid_frame(std::string &&str, int error_code) :
    exception(std::move(str)), _error_code(error_code) {}

invalid_frame::invalid_frame(std::string_view str, int error_code) :
    exception(str), _error_code(error_code) {}

int invalid_frame::error_code() const noexcept {
    return _error_code;
}

void frame_t::clear() {
    fin  = false,
    rsv1 = false,
    rsv2 = false,
    rsv3 = false;
    opcode = OPCODE_CONT;
    length = 0;
    payload.clear();
}

std::pair<size_t, std::optional<frame_t> > frame_parser::process(const char *data, size_t len,
                                                                 bool masked, uint64_t max_payload_len) {
    // restore state
    int stage = _state.stage;
    size_t bytes_remaining = _state.bytes_remaining;
    uint64_t tmp64 = _state.tmp64;
    size_t mask_pos = _state.mask_pos;
    uint32_t mask = _state.mask;

    size_t i = 0, read_len = 0;
    unsigned char byte, tmp;

    switch (stage) {
    case 0:     // flags
        if (i >= len)
            goto save_state;

        byte = data[i++];
        _frame.fin  = byte & 0b10000000;
        _frame.rsv1 = byte & 0b01000000;
        _frame.rsv2 = byte & 0b00100000;
        _frame.rsv3 = byte & 0b00010000;
        _frame.opcode = opcode_t(byte & 0b1111);

        // check for reserved flags or opcodes
        // TODO: allow reserved parts based on active extensions
        if (byte & 0b01110000)
            throw invalid_frame("using reserved flags"sv, 1002);

        if (OPCODE_RSV3 <= _frame.opcode && _frame.opcode <= OPCODE_RSV7)
            throw invalid_frame("using reserved data opcode"sv, 1003);

        if (OPCODE_RSVB <= _frame.opcode)
            throw invalid_frame("using reserved control opcode"sv, 1002);

        // control frames cannot be fragmented
        if (!_frame.fin && _frame.opcode >= OPCODE_CLOSE)
            throw invalid_frame("control frames cannot be fragmented"sv, 1002);

        stage = 1;
        [[fallthrough]];

    case 1:     // mask bit and short payload length
        if (i >= len)
            goto save_state;

        // mask bit
        byte = data[i++];
        // make sure mask presence matches what we expect
        if ((byte & 0b10000000) != masked) {
            if (masked)
                throw invalid_frame("received unmasked frame"sv, 1002);
            else
                throw invalid_frame("received masked frame"sv, 1002);
        }

        // payload length
        tmp = byte & 0b01111111;
        if (tmp == 127)         // extended 64-bit length
            bytes_remaining = 8;
        else if (tmp == 126)    // extended 16-bit length
            bytes_remaining = 2;
        else                    // short 7-bit length
            _frame.length = tmp;

        // control frames can be at most 125 bytes long
        if (_frame.opcode >= OPCODE_CLOSE && tmp > 125)
            throw invalid_frame("payload length of control frame exceeds 125 bytes"sv, 1009);

        if (bytes_remaining) {
            stage = 2;

        case 2:     // extended payload length
            while (bytes_remaining) {
                if (i >= len)
                    goto save_state;

                tmp64 = (tmp64 << 8u) | (unsigned char) data[i++];
                bytes_remaining--;
            }

            _frame.length = tmp64;
            tmp64 = 0;
        }

        // make sure it doesn't exceed the maximum length
        if (_frame.length > max_payload_len)
            throw invalid_frame("payload length exceeds max limit"sv, 1009);

        if (masked) {
            stage = 3;

        case 3:     // masking key
            while (mask_pos < 4) {
                if (i >= len)
                    goto save_state;

                byte = data[i++];
                mask = (mask << 8) | byte;
                mask_pos++;
            }

            // start mask from most significant byte later
            mask_pos = 3;
        }

        stage = 4;
        bytes_remaining = _frame.length;
        [[fallthrough]];

    case 4:     // payload
        // TODO: for text data, verify that it's valid UTF-8
        // (maybe outside of this function after all pieces have been combined)
        if (masked) {
            // read and unmask bytes
            // TODO: optimize for aligned 4-byte chunks, when the mask can be applied in one go, or even bigger chunks
            while (bytes_remaining) {
                if (i >= len)
                    goto save_state;

                tmp = mask >> (4 * mask_pos);
                mask_pos = (mask_pos + 3) % 4;  // mask_pos-- but always in range [0,4)
                _frame.payload.push_back(data[i++] ^ tmp);
                bytes_remaining--;
            }
        } else {
            // read raw bytes without masking
            read_len = std::min(len - i, bytes_remaining);
            _frame.payload.append(data + i, read_len);
            i += read_len;
            bytes_remaining -= read_len;

            // more data left
            if (bytes_remaining)
                goto save_state;
        }

        {
            // full frame read, reset state
            std::optional<frame_t> frame = std::move(_frame);
            clear();

            // return bytes processed and frame
            return {i, std::move(frame)};
        }

    default:
        throw http::internal_error("websocket frame parser reached an invalid state"sv);
    }

save_state:
    // save state
    _state.stage = stage;
    _state.bytes_remaining = bytes_remaining;
    _state.tmp64 = tmp64;
    _state.mask_pos = mask_pos;
    _state.mask = mask;

    // return bytes processed
    return {i, std::nullopt};
}

void frame_parser::clear() {
    _state.stage = 0;
    _state.bytes_remaining = 0;
    _state.tmp64 = 0;
    _state.mask_pos = 0;
    _state.mask = 0;
    _frame.clear();
}

void frame_send(networking::tcp_socket &socket, opcode_t opcode, bool fin, bool masked, std::string_view payload,
                bool rsv1, bool rsv2, bool rsv3) {
    std::string header;

    // flags and opcode
    header.push_back((fin << 7u) | (rsv1 << 6u) | (rsv2 << 5u) | (rsv3 << 4u) | opcode);

    // mask bit and payload length
    if (payload.length() < 126) {           // short 7-bit length
        header.push_back((masked << 7u) | payload.length());
    } else if (payload.length() < 65536) {  // extended 16-bit length
        header.push_back((masked << 7u) | 126u);
        header.push_back(payload.length() >> 8);
        header.push_back(payload.length());
    } else {                                // extended 64-bit length
        header.push_back((masked << 7u) | 127u);
        header.push_back(payload.length() >> 56);
        header.push_back(payload.length() >> 48);
        header.push_back(payload.length() >> 40);
        header.push_back(payload.length() >> 32);
        header.push_back(payload.length() >> 24);
        header.push_back(payload.length() >> 16);
        header.push_back(payload.length() >> 8);
        header.push_back(payload.length());
    }

    if (masked) {
        // generate 32-bit masking key
        char masking_key[4];

        if (RAND_bytes((unsigned char*) masking_key, sizeof(masking_key)) != 1) {
            unsigned long m_err = ERR_get_error();
            throw http::internal_error("websocket frame send: failed to generate masking key: "s +
                                       "[error " + std::to_string(m_err) + "] " +
                                       ERR_lib_error_string(m_err) + ": " +
                                       ERR_reason_error_string(m_err));
        }

        header.append(masking_key, sizeof(masking_key));
        socket.send(header);

        // send masked payload
        // TODO: optimize masking in chunks, and send in chunks
        std::string payload_masked;
        payload_masked.reserve(payload.length());

        for (size_t i = 0; i < payload.length(); i++)
            payload_masked.push_back(payload[i] ^ masking_key[i % sizeof(masking_key)]);

        socket.send(payload_masked);
        socket.flush();

    } else {
        // send without masking
        socket.send(header);
        socket.send(payload.data(), payload.length());
        socket.flush();
    }
}

void frame_send_close(networking::tcp_socket &socket, bool masked,
                      unsigned short error_code, std::string_view message) {
    std::string payload;
    payload.push_back(error_code >> 8);
    payload.push_back(error_code);
    payload.append(message.data(), message.length());
    frame_send(socket, OPCODE_CLOSE, true, masked, payload);
}

void message_t::clear() {
    type = OPCODE_CONT;
    data.clear();
}

base::base() {}

base::~base() {
    // TODO: stop thread
}

const std::string& base::subprotocol_used() const {
    // if empty, then no subprotocol is being used
    return _subprotocol_used;
}

void base::_thread_fn(bool is_client) {
    try {
        message_t message_in;
        bool close_sent = false;

        while (true) {
            // TODO: backpressure
            // receive from network
            auto [recv_data, recv_len] = _socket->recv();

            if (recv_len == 0)
                // TODO: handle as error
                return;

            // process received data
            while (recv_len) {
                auto [processed, frame_opt] = _frame_parser.process(recv_data, recv_len, !is_client,
                                                                    STRTB_WEBSOCKET_MAX_FRAME_LEN);
                recv_data += processed;
                recv_len -= processed;

                // entire frame parsed
                if (frame_opt) {
                    const auto &frame = frame_opt.value();

                    bool message_complete = false;

                    switch (frame.opcode) {
                    case OPCODE_CONT:   // continuation data frame
                        if (message_in.type == OPCODE_CONT)
                            throw invalid_frame("invalid message fragmentation", 1002);
                        if (message_in.data.length() + frame.payload.length() > STRTB_WEBSOCKET_MAX_MESSAGE_LEN)
                            throw invalid_frame("message too big", 1009);

                        message_in.data.append(frame.payload.data(), frame.payload.length());
                        message_complete = frame.fin;
                        break;

                    case OPCODE_TEXT:   // text data frame
                    case OPCODE_BIN:
                        if (message_in.type != OPCODE_CONT)
                            throw invalid_frame("invalid message fragmentation", 1002);
                        if (message_in.data.length() + frame.payload.length() > STRTB_WEBSOCKET_MAX_MESSAGE_LEN)
                            throw invalid_frame("message too big", 1009);

                        message_in.type = frame.opcode;
                        message_in.data.assign(frame.payload.data(), frame.payload.length());
                        message_complete = frame.fin;
                        break;

                    case OPCODE_CLOSE:  // close control frame
                        // TODO: close gracefully (different for clients and servers)
                        // echo back the close frame
                        if (!close_sent)
                            frame_send(*_socket, OPCODE_CLOSE, true, is_client, frame.payload);

                        // TODO: force close if it takes too long
                        if (is_client) {
                            // the client waits for the server to close the connection before closing its side
                            while (_socket->recv().second);

                            if (typeid(*_socket) == typeid(networking::tcp_client_ssl))
                                static_cast<networking::tcp_client_ssl*>(_socket)->shutdown_gracefully();

                            _socket->close();
                            // TODO: additional cleanup if needed
                        } else {
                            // the server instantly closes its side of the connection
                            if (typeid(*_socket) == typeid(networking::tcp_server_connection_ssl))
                                static_cast<networking::tcp_server_connection_ssl*>(_socket)->shutdown_gracefully();
                            else
                                _socket->shutdown(true, false);

                            // wait for the client to also close its side before fully closing
                            while (_socket->recv().second);
                            _socket->close();

                            // TODO: additional cleanup if needed
                        }
                        return;

                    case OPCODE_PING:   // ping control frame
                        frame_send(*_socket, OPCODE_PONG, true, is_client, frame.payload);
                        break;

                    case OPCODE_PONG:   // pong control frame
                        break;

                    default:            // should never happen, frame_parser handles reserved frames
                        throw http::internal_error("unhandled frame opcode");
                    }

                    // full message received and reconstructed
                    if (message_complete) {
                        std::lock_guard lock(_mutex);
                        _queue_in.push(std::move(message_in));
                        message_in.clear();
                        _cv_in.notify_one();
                    }
                }
            }
        }
    } catch (invalid_frame &e) {
        // TODO: save error condition
        // close connection
        frame_send_close(*_socket, is_client, e.error_code(), e.what());
    } catch (...) {
        // TODO: cleanup
        // TODO: save exception or error in some way
    }
}

std::optional<message_t> base::recv() {
    std::unique_lock<std::mutex> _lock(_mutex);
    // TODO: check if the connection is closed

    // wait for a message to be received by the websocket thread
    while (_queue_in.empty()) {
        // TODO: also check for errors
        _cv_in.wait(_lock);
    }

    std::optional<message_t> msg = std::move(_queue_in.front());
    _queue_in.pop();
    return msg;
}

}