#ifndef STRTB_UNICODE_UNICODE_H
#define STRTB_UNICODE_UNICODE_H

#include <cstdint>
#include <string>

namespace strtb::unicode {

std::string codepoint_to_utf8(uint32_t codepoint);

}

#endif // STRTB_UNICODE_UNICODE_H
