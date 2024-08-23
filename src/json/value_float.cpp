#include "value_float.h"
#include <limits>
#include <sstream>

using namespace json;

value_float::value_float() : json::value(VAL_FLOAT), _value(0) {};

value_float::value_float(const double value) : json::value(VAL_FLOAT), _value(value) {deinf();}

double value_float::value() const {return _value;}

void value_float::set_value(const double value) {
    _value = value;
    deinf();
}

value* value_float::copy() const {return new value_float(_value);}

void value_float::deinf() {
    // Since JSON numbers don't support infinity, convert INF to min/max limits of doubles, based on sign
    if (_value == std::numeric_limits<double>::infinity())
        _value = std::numeric_limits<double>::max();
    else if (_value == -std::numeric_limits<double>::infinity())
        _value = std::numeric_limits<double>::min();
}

void value_float::write_to_stream(std::ostream &stream, int pretty_print, int pretty_print_level, const char* newline) const {
    // JSON requires using '.' as a decimal point, which might be different
    // from our locale's decimal point, so we need to work around this
    // by creating a fake (string) stream, giving it the standard C locale
    // and doing our float-to-string conversion there.
    std::stringstream str_stream(std::ios_base::out);
    str_stream.imbue(std::locale("C"));
    str_stream << _value;
    stream << str_stream.str();
}
