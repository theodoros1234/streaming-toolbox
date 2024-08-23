#include "strescape.h"

#include <ios>
#include <iomanip>
#include <sstream>

using namespace common;

std::string common::char_escape(char c, char escape_quote_char, bool quoted) {
    std::string escaped;

    // Opening quote
    if (quoted)
        escaped.push_back(escape_quote_char);

    // Character that might need escaping
    switch (c) {
        break;
    case '\\':      // Backslash
        escaped.append("\\\\");
        break;
    case '\b':      // Backspace
        escaped.append("\\b");
        break;
    case '\f':      // Formfeed
        escaped.append("\\f");
        break;
    case '\n':      // Newline
        escaped.append("\\n");
        break;
    case '\r':      // Carriage return
        escaped.append("\\r");
        break;
    case '\t':      // Horizontal tab
        escaped.append("\\t");
        break;
    default:
        if (c == escape_quote_char) {       // Quotation mark
            escaped.push_back('\\');
            escaped.push_back(escape_quote_char);
        } else if (0 <= c && c <= 0x1f) {   // Other control character
            // Convert to hex escape code
            std::stringstream str_stream(std::ios_base::out);
            str_stream << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
            escaped.append(str_stream.str());
        } else  {   // Regular printable character
            escaped.push_back(c);
        }
    }

    // Closing quote
    if (quoted)
        escaped.push_back(escape_quote_char);

    return escaped;
}

std::string common::string_escape(const char* str, char escape_quote_char, bool quoted) {
    std::string escaped;

    // Opening quote
    if (quoted)
        escaped.push_back(escape_quote_char);

    // Characters that might need escaping
    while (*str != 0) {
        escaped.append(char_escape(*str, escape_quote_char, false));
        str++;
    }

    // Closing quote
    if (quoted)
        escaped.push_back(escape_quote_char);

    return escaped;
}

std::string common::string_escape(const std::string &str, char escape_quote_char, bool quoted) {
    std::string escaped;

    // Opening quote
    if (quoted)
        escaped.push_back(escape_quote_char);

    // Characters that might need escaping
    for (char c : str)
        escaped.append(char_escape(c, escape_quote_char, false));

    // Closing quote
    if (quoted)
        escaped.push_back(escape_quote_char);

    return escaped;
}

