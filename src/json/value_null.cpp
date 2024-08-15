#include "value_null.h"

using namespace json;

value_null::value_null() : value(VAL_NULL) {}

value* value_null::copy() const {return new value_null();}

void value_null::write_to_stream(std::ostream &stream, int pretty_print, int pretty_print_level, const char* newline) const {
    stream << "null";
}
