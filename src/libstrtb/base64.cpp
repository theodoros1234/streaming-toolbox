#include "base64.h"
#include "strescape.h"
#include <cassert>
#include <stdexcept>

namespace strtb {

static inline char encode_bits(int b) {
    assert(0 <= b && b < 64);
    if (b < 26)
        return 'A' + b;
    else if (b < 52)
        return 'a' + (b - 26);
    else if (b < 62)
        return '0' + (b - 52);
    else if (b == 62)
        return '+';
    else
        return '/';
}

static inline int decode_bits(char b) {
    if ('A' <= b && b <= 'Z')
        return b - 'A';
    else if ('a' <= b && b <= 'z')
        return b - 'a' + 26;
    else if ('0' <= b && b <= '9')
        return b - '0' + 52;
    else if (b == '+')
        return 62;
    else if (b == '/')
        return 63;
    else if (b == '=')
        throw std::invalid_argument("improper padding");
    else
        throw std::invalid_argument("invalid character " + char_escape(b));
}

static inline char encode_bits_url(int b) {
    assert(0 <= b && b < 64);
    if (b < 26)
        return 'A' + b;
    else if (b < 52)
        return 'a' + (b - 26);
    else if (b < 62)
        return '0' + (b - 52);
    else if (b == 62)
        return '-';
    else
        return '_';
}

static inline int decode_bits_url(char b) {
    if ('A' <= b && b <= 'Z')
        return b - 'A';
    else if ('a' <= b && b <= 'z')
        return b - 'a' + 26;
    else if ('0' <= b && b <= '9')
        return b - '0' + 52;
    else if (b == '-')
        return 62;
    else if (b == '_')
        return 63;
    else if (b == '=')
        throw std::invalid_argument("improper padding");
    else
        throw std::invalid_argument("invalid character " + char_escape(b));
}

static inline std::string base64_encode_fn(std::string_view str, char (*encode_bits_fn)(int)) {
    std::string out;
    size_t i;

    // reserve the needed memory with padding in mind
    out.reserve((((str.length() * 8) + 23) / 24) * 4);

    // complete 24-bit groups
    for (i = 0; i+2 < str.length(); i += 3) {
        // split into 6-bit parts
        int split6[4] = {
              str[i]   >> 2,
            ((str[i]   << 4) & 0b111111) | (str[i+1] >> 4),
            ((str[i+1] << 2) & 0b111111) | (str[i+2] >> 6),
              str[i+2]       & 0b111111
        };

        // encode into base64
        char encoded[4] = {
            encode_bits_fn(split6[0]),
            encode_bits_fn(split6[1]),
            encode_bits_fn(split6[2]),
            encode_bits_fn(split6[3])
        };
        out.append(encoded, 4);
    }

    // exit if fully encoded
    if (i >= str.length())
        return out;

    // last incomplete group
    // split into 6-bit groups from remaining characters
    int split6[3] = {0};
    split6[0] = str[i] >> 2;
    split6[1] = (str[i] << 4) & 0b111111;

    if (i+1 < str.length()) {
        split6[1] |= str[i+1] >> 4;
        split6[2] = (str[i+1] << 2) & 0b111111;
    }

    // encode with padding
    char encoded[4] = {
        encode_bits_fn(split6[0]),
        encode_bits_fn(split6[1]),
        i+1 < str.length() ? encode_bits_fn(split6[2]) : '=',
        '='
    };

    out.append(encoded, 4);
    return out;
}

static inline std::string base64_decode_fn(std::string_view str, bool strict_padding, int (*decode_bits_fn)(char)) {
    std::string out;
    size_t i;

    // empty input
    if (str.empty())
        return out;

    // make sure the input has a proper length
    if (strict_padding && str.length() % 4)
        throw std::invalid_argument("improper padding");

    // reserve the needed memory
    out.reserve((str.length() * 6) / 8);

    // process all chunks except the last one
    for (i = 0; i+4 < str.length(); i += 4) {
        // decode into 6-bit parts
        int split6[4] = {
            decode_bits_fn(str[i]),
            decode_bits_fn(str[i+1]),
            decode_bits_fn(str[i+2]),
            decode_bits_fn(str[i+3])
        };

        // reconstruct octets
        char decoded[3] = {
            char( (split6[0] << 2)               | (split6[1] >> 4)),
            char(((split6[1] << 4) & 0b11111111) | (split6[2] >> 2)),
            char(((split6[2] << 6) & 0b11111111) |  split6[3])
        };
        out.append(decoded, 3);
    }

    // handle final group
    // check padding
    size_t pad_start = i+4;
    while ((i < pad_start) && (pad_start - 1 >= str.length() || str[pad_start - 1] == '='))
        pad_start--;

    if (pad_start <= i + 1)
        throw std::invalid_argument("improper padding");

    // decode into 6-bit parts
    int split6[4] = {
        decode_bits_fn(str[i]),
        decode_bits_fn(str[i+1]),
        i+2 < pad_start ? decode_bits_fn(str[i+2]) : 0,
        i+3 < pad_start ? decode_bits_fn(str[i+3]) : 0,
    };

    // reconstruct octets
    char decoded[3] = {
        char( (split6[0] << 2)               | (split6[1] >> 4)),
        char(((split6[1] << 4) & 0b11111111) | (split6[2] >> 2)),
        char(((split6[2] << 6) & 0b11111111) |  split6[3])
    };

    // check for invalid pad bits
    if (strict_padding &&
        ((i+2 >= pad_start && decoded[1]) ||
         (i+3 >= pad_start && decoded[2])))
        throw std::invalid_argument("improper padding");

    out.append(decoded, pad_start - 1 - i);

    return out;
}

std::string base64_encode(std::string_view str) {
    return base64_encode_fn(str, encode_bits);
}

std::string base64_decode(std::string_view str, bool strict_padding) {
    return base64_decode_fn(str, strict_padding, decode_bits);
}

std::string base64url_encode(std::string_view str) {
    return base64_encode_fn(str, encode_bits_url);
}

std::string base64url_decode(std::string_view str, bool strict_padding) {
    return base64_decode_fn(str, strict_padding, decode_bits_url);
}

}