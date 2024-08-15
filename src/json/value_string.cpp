#include "value_string.h"
#include <iomanip>
#include <sstream>

using namespace json;

value_string::value_string(const char* value) : json::value(VAL_STRING), _value(value) {}

value_string::value_string(const std::string &value) : json::value(VAL_STRING), _value(value) {}

std::string value_string::value() const {return _value;}

void value_string::set_value(const char* value) {_value = std::string(value);}

void value_string::set_value(const std::string &value) {_value = value;}

value* value_string::copy() const {return new value_string(_value.c_str());}

void value_string::write_to_stream(std::ostream &stream, int pretty_print, int pretty_print_level, const char* newline) const {
    stream << '"';
    for (auto c : _value) {
        switch (c) {
        case '"':       // Quotation mark
            stream << "\\\"";
            break;
        case '\\':      // Backslash
            stream << "\\\\";
            break;
        case '\b':      // Backspace
            stream << "\\b";
            break;
        case '\f':      // Formfeed
            stream << "\\f";
            break;
        case '\n':      // Newline
            stream << "\\n";
            break;
        case '\r':      // Carriage return
            stream << "\\r";
            break;
        case '\t':      // Horizontal tab
            stream << "\\t";
            break;
        default:
            if (0 <= c && c <= 0x1f) {  // Other control character
                // Convert to hex escape code
                std::stringstream str_stream(std::ios_base::out);
                str_stream << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                stream << str_stream.str();
            } else  {   // Regular printable character
                stream << c;
            }
        }
    }
    stream << '"';
}
