#include "value_string.h"
#include "../common/strescape.h"

using namespace strtb;
using namespace strtb::json;

value_string::value_string() : json::value(VAL_STRING) {};

value_string::value_string(const char* value) : json::value(VAL_STRING), _value(value) {}

value_string::value_string(const std::string &value) : json::value(VAL_STRING), _value(value) {}

std::string value_string::value() const {return _value;}

void value_string::set_value(const char* value) {_value = std::string(value);}

void value_string::set_value(const std::string &value) {_value = value;}

value* value_string::copy() const {return new value_string(_value.c_str());}

void value_string::write_to_stream(std::ostream &stream, int pretty_print, int pretty_print_level, const char* newline) const {
    stream << common::string_escape(_value);
}
