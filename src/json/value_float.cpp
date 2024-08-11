#include "value_float.h"

using namespace json;

value_float::value_float(const double value) : json::value(VAL_FLOAT), _value(value) {}

double value_float::value() const {return _value;}

void value_float::set_value(const double value) {_value = value;}

value* value_float::copy() const {return new value_float(_value);}
