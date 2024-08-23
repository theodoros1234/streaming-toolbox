#include "value_int.h"

using namespace json;

value_int::value_int() : json::value(VAL_INT), _value(0) {};

value_int::value_int(const long long value) : json::value(VAL_INT), _value(value) {}

long long value_int::value() const {return _value;}

void value_int::set_value(const long long value) {_value = value;}

value* value_int::copy() const {return new value_int(_value);}

void value_int::write_to_stream(std::ostream &stream, int pretty_print, int pretty_print_level, const char* newline) const {
    stream << _value;
}
