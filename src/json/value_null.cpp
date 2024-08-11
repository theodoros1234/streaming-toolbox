#include "value_null.h"

using namespace json;

value_null::value_null() : value(VAL_NULL) {}

value* value_null::copy() const {return new value_null();}
