#include "value_string.h"

using namespace json;

value_string::value_string(const char* value) : json::value(VAL_STRING), _value(value) {}

value_string::value_string(const std::string &value) : json::value(VAL_STRING), _value(value) {}

void value_string::set_value(const char* value) {_value = std::string(value);}

void value_string::set_value(const std::string &value) {_value = value;}

value* value_string::copy() const {return new value_string(_value.c_str());}
