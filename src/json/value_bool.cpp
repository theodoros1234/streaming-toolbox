#include "value_bool.h"

using namespace json;

value_bool::value_bool(const bool value) : json::value(VAL_BOOL), _value(value) {}

void value_bool::set_value(const bool value) {_value = value;}

value* value_bool::copy() const {return new value_bool(_value);}

void value_bool::write_to_stream(std::ostream &stream, int pretty_print, int pretty_print_level, const char* newline) const {
    if (_value)
        stream << "true";
    else
        stream << "false";
}
