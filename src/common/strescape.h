#ifndef STRESCAPE_H
#define STRESCAPE_H

#include <string>

namespace strtb::common {

std::string char_escape(char c, char escape_quote_char = '\'', bool quoted = true);
std::string string_escape(const char* str, char escape_quote_char = '"', bool quoted = true);
std::string string_escape(const std::string &str, char escape_quote_char = '"', bool quoted = true);

}

#endif // STRESCAPE_H
