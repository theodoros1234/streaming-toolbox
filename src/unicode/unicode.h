#ifndef UNICODE_H
#define UNICODE_H

#include <cstdint>
#include <string>

namespace strtb::unicode {

std::string codepoint_to_utf8(uint32_t codepoint);

}

#endif // UNICODE_H
