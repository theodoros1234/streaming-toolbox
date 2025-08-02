#include "cast.h"

using namespace strtb::json;

wrong_type::wrong_type(val_type expected, val_type got)
    : expected(expected), got(got),
    _what("expected type " + val_type_str(expected) + " but got " + val_type_str(got)) {}

const char* wrong_type::what() const noexcept {return _what.c_str();}

const char* nullptr_exception::what() const noexcept {return "null pointer was given";}

value_null* strtb::json::cast_null(value* val) {
    if (val == nullptr)
        throw nullptr_exception();
    if (val->type() != VAL_NULL)
        throw wrong_type(VAL_NULL, val->type());
    return (value_null*) val;
}

value_bool* strtb::json::cast_bool(value* val) {
    if (val == nullptr)
        throw nullptr_exception();
    if (val->type() != VAL_BOOL)
        throw wrong_type(VAL_BOOL, val->type());
    return (value_bool*) val;
}

value_int* strtb::json::cast_int(value* val) {
    if (val == nullptr)
        throw nullptr_exception();
    if (val->type() != VAL_INT)
        throw wrong_type(VAL_INT, val->type());
    return (value_int*) val;
}

value_float* strtb::json::cast_float(value* val) {
    if (val == nullptr)
        throw nullptr_exception();
    if (val->type() != VAL_FLOAT)
        throw wrong_type(VAL_FLOAT, val->type());
    return (value_float*) val;
}

value_string* strtb::json::cast_string(value* val) {
    if (val == nullptr)
        throw nullptr_exception();
    if (val->type() != VAL_STRING)
        throw wrong_type(VAL_STRING, val->type());
    return (value_string*) val;
}

value_array* strtb::json::cast_array(value* val) {
    if (val == nullptr)
        throw nullptr_exception();
    if (val->type() != VAL_ARRAY)
        throw wrong_type(VAL_ARRAY, val->type());
    return (value_array*) val;
}

value_object* strtb::json::cast_object(value* val) {
    if (val == nullptr)
        throw nullptr_exception();
    if (val->type() != VAL_OBJECT)
        throw wrong_type(VAL_OBJECT, val->type());
    return (value_object*) val;
}

const value_null* strtb::json::cast_null(const value* val) {
    if (val == nullptr)
        throw nullptr_exception();
    if (val->type() != VAL_NULL)
        throw wrong_type(VAL_NULL, val->type());
    return (const value_null*) val;
}

const value_bool* strtb::json::cast_bool(const value* val) {
    if (val == nullptr)
        throw nullptr_exception();
    if (val->type() != VAL_BOOL)
        throw wrong_type(VAL_BOOL, val->type());
    return (const value_bool*) val;
}

const value_int* strtb::json::cast_int(const value* val) {
    if (val == nullptr)
        throw nullptr_exception();
    if (val->type() != VAL_INT)
        throw wrong_type(VAL_INT, val->type());
    return (const value_int*) val;
}

const value_float* strtb::json::cast_float(const value* val) {
    if (val == nullptr)
        throw nullptr_exception();
    if (val->type() != VAL_FLOAT)
        throw wrong_type(VAL_FLOAT, val->type());
    return (const value_float*) val;
}

const value_string* strtb::json::cast_string(const value* val) {
    if (val == nullptr)
        throw nullptr_exception();
    if (val->type() != VAL_STRING)
        throw wrong_type(VAL_STRING, val->type());
    return (const value_string*) val;
}

const value_array* strtb::json::cast_array(const value* val) {
    if (val == nullptr)
        throw nullptr_exception();
    if (val->type() != VAL_ARRAY)
        throw wrong_type(VAL_ARRAY, val->type());
    return (const value_array*) val;
}

const value_object* strtb::json::cast_object(const value* val) {
    if (val == nullptr)
        throw nullptr_exception();
    if (val->type() != VAL_OBJECT)
        throw wrong_type(VAL_OBJECT, val->type());
    return (const value_object*) val;
}
