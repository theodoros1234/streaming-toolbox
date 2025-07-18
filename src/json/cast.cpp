#include "cast.h"

using namespace strtb::json;

wrong_type::wrong_type(val_type expected, val_type got)
    : expected(expected), got(got),
    _what("expected type " + val_type_str(expected) + " but got " + val_type_str(got)) {}

const char* wrong_type::what() const noexcept {return _what.c_str();}

value_null* strtb::json::cast_null(value* val, bool except) {
    if (val->type() == VAL_NULL)
        return (value_null*) val;
    else if (except)
        throw wrong_type(VAL_NULL, val->type());
    else
        return NULL;
}

value_bool* strtb::json::cast_bool(value* val, bool except) {
    if (val->type() == VAL_BOOL)
        return (value_bool*) val;
    else if (except)
        throw wrong_type(VAL_BOOL, val->type());
    else
        return NULL;
}

value_int* strtb::json::cast_int(value* val, bool except) {
    if (val->type() == VAL_INT)
        return (value_int*) val;
    else if (except)
        throw wrong_type(VAL_INT, val->type());
    else
        return NULL;
}

value_float* strtb::json::cast_float(value* val, bool except) {
    if (val->type() == VAL_FLOAT)
        return (value_float*) val;
    else if (except)
        throw wrong_type(VAL_FLOAT, val->type());
    else
        return NULL;
}

value_string* strtb::json::cast_string(value* val, bool except) {
    if (val->type() == VAL_STRING)
        return (value_string*) val;
    else if (except)
        throw wrong_type(VAL_STRING, val->type());
    else
        return NULL;
}

value_array* strtb::json::cast_array(value* val, bool except) {
    if (val->type() == VAL_ARRAY)
        return (value_array*) val;
    else if (except)
        throw wrong_type(VAL_ARRAY, val->type());
    else
        return NULL;
}

value_object* strtb::json::cast_object(value* val, bool except) {
    if (val->type() == VAL_OBJECT)
        return (value_object*) val;
    else if (except)
        throw wrong_type(VAL_OBJECT, val->type());
    else
        return NULL;
}


