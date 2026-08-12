#ifndef STRTB_BASE64_H
#define STRTB_BASE64_H

#include <string>

namespace strtb {

std::string base64_encode(std::string_view str);
std::string base64_decode(std::string_view str, bool strict_padding = true);
std::string base64url_encode(std::string_view str);
std::string base64url_decode(std::string_view str, bool strict_padding = true);

}

#endif // STRTB_BASE64_H
