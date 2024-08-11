#include "value_bool.h"

using namespace json;

value_bool::value_bool(const bool value) : json::value(VAL_BOOL), _value(value) {}

void value_bool::set_value(const bool value) {_value = value;}

value* value_bool::copy() const {return new value_bool(_value);}
