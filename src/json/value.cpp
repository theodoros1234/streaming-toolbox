#include "value.h"

using namespace json;

value::value(val_type type) : _type(type) {}

val_type value::type() const {return this->_type;}
