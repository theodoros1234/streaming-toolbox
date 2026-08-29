#ifndef STRTB_WEBSOCKET_PROTOCOL_H
#define STRTB_WEBSOCKET_PROTOCOL_H

#include <cstdint>
#include <string>
#include <optional>
#include <utility>

#include "../http/protocol.h"
#include "../networking/tcp_socket.h"

namespace strtb::websocket {

class invalid_frame : public http::exception {
private:
    int _error_code = 0;
public:
    invalid_frame(const char *str, int error_code);
    invalid_frame(const std::string &str, int error_code);
    invalid_frame(std::string &&str, int error_code);
    invalid_frame(std::string_view str, int error_code);
    int error_code() const noexcept;
};

enum opcode_t {
    OPCODE_CONT,
    OPCODE_TEXT,
    OPCODE_BIN,
    OPCODE_RSV3,
    OPCODE_RSV4,
    OPCODE_RSV5,
    OPCODE_RSV6,
    OPCODE_RSV7,
    OPCODE_CLOSE,
    OPCODE_PING,
    OPCODE_PONG,
    OPCODE_RSVB,
    OPCODE_RSVC,
    OPCODE_RSVD,
    OPCODE_RSVE,
    OPCODE_RSVF
};

struct frame_t {
    bool fin = false,
        rsv1 = false,
        rsv2 = false,
        rsv3 = false;
    opcode_t opcode = OPCODE_CONT;
    uint64_t length = 0;
    std::string payload;
    void clear();
};

class frame_parser {
private:
    frame_t _frame;
    struct state {
        int stage = 0;
        size_t bytes_remaining = 0;
        uint64_t tmp64 = 0;
        size_t mask_pos = 0;
        uint32_t mask = 0;
    } _state;

public:
    // returns [bytes processed, parsed frame (if any)]
    std::pair<size_t, std::optional<frame_t> > process(const char *data, size_t len,
                                                       bool masked, uint64_t max_payload_len);
    void clear();
};

void frame_send(networking::tcp_socket &socket, opcode_t opcode, bool fin, bool masked, std::string_view payload,
                bool rsv1=false, bool rsv2=false, bool rsv3=false);

}

#endif // STRTB_WEBSOCKET_PROTOCOL_H
