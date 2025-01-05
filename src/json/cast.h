#ifndef STRTB_JSON_CAST_H
#define STRTB_JSON_CAST_H

#include <exception>
#include <string>
#include "all_value_types.h"

namespace strtb::json {

class wrong_type : public std::exception {
public:
    const val_type expected, got;
    wrong_type(val_type expected, val_type got);
    const char* what() const noexcept;
private:
    std::string _what;
};

value_null* cast_null(value* val, bool except = true);
value_bool* cast_bool(value* val, bool except = true);
value_int* cast_int(value* val, bool except = true);
value_float* cast_float(value* val, bool except = true);
value_string* cast_string(value* val, bool except = true);
value_array* cast_array(value* val, bool except = true);
value_object* cast_object(value* val, bool except = true);

}

#endif // STRTB_JSON_CAST_H
