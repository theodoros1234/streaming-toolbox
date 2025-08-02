#ifndef STRTB_JSON_CAST_H
#define STRTB_JSON_CAST_H

#include <string>
#include "all_value_types.h"

namespace strtb::json {

class wrong_type : public json_error {
public:
    const val_type expected, got;
    wrong_type(val_type expected, val_type got);
    const char* what() const noexcept;
private:
    std::string _what;
};

class nullptr_exception : public json_error {
public:
    const char* what() const noexcept;
};

value_null* cast_null(value* val);
value_bool* cast_bool(value* val);
value_int* cast_int(value* val);
value_float* cast_float(value* val);
value_string* cast_string(value* val);
value_array* cast_array(value* val);
value_object* cast_object(value* val);
const value_null* cast_null(const value* val);
const value_bool* cast_bool(const value* val);
const value_int* cast_int(const value* val);
const value_float* cast_float(const value* val);
const value_string* cast_string(const value* val);
const value_array* cast_array(const value* val);
const value_object* cast_object(const value* val);

}

#endif // STRTB_JSON_CAST_H
