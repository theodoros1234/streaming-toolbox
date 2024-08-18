#include "unicode.h"

using namespace unicode;

std::string unicode::codepoint_to_utf8(uint32_t codepoint) {
    if (0xD800 <= codepoint && codepoint <= 0xDFFF) {
        // Invalid code point (surrogates)
        return "�";
    } else if (codepoint <= 0x7F) {
        // One-byte sequence
        char c[2];
        c[0] = codepoint;
        c[1] = 0;
        return c;
    } else if (codepoint <= 0x07FF) {
        // Two-byte sequence
        char c[3];
        c[0] = 0b11000000 | ((codepoint >> 6)  & 0b00011111);
        c[1] = 0b10000000 | ( codepoint        & 0b00111111);
        c[2] = 0;
        return c;
    } else if (codepoint <= 0xFFFF) {
        // Three-byte sequence
        char c[4];
        c[0] = 0b11100000 | ((codepoint >> 12) & 0b00001111);
        c[1] = 0b10000000 | ((codepoint >> 6 ) & 0b00111111);
        c[2] = 0b10000000 | ( codepoint        & 0b00111111);
        c[3] = 0;
        return c;
    } else if (codepoint <= 0x10FFFF) {
        // Four-byte sequence
        char c[5];
        c[0] = 0b11110000 | ((codepoint >> 18) & 0b00000111);
        c[1] = 0b10000000 | ((codepoint >> 12) & 0b00111111);
        c[2] = 0b10000000 | ((codepoint >> 6 ) & 0b00111111);
        c[3] = 0b10000000 | ( codepoint        & 0b00111111);
        c[4] = 0;
        return c;
    } else {
        // Invalid code point (out of valid range)
        return "�";
    }
}
