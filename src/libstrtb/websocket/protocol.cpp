#include "protocol.h"

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace strtb::websocket {

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
    size_t mask_pos = _state.mask_pos;
    uint32_t mask = _state.mask;

    size_t i = 0, read_len = 0;
    unsigned char byte, tmp;
    uint64_t tmp64 = 0;

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

}