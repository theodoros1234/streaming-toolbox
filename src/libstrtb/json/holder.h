#ifndef STRTB_JSON_HOLDER_H
#define STRTB_JSON_HOLDER_H

#include "all_value_types.h"

namespace strtb::json {

class holder {
private:
    json::value* _v = nullptr;
public:
    holder();
    holder(const holder& o);
    holder(holder&& o);
    holder(json::value* v);
    holder(const value_auto& v);
    holder(val_type new_type);
    ~holder();
    holder& operator=(json::value* other);
    holder& operator=(const json::holder& other);
    holder& operator=(json::holder&& other);
    holder& operator=(const value_auto& v);
    holder& operator=(val_type new_type);
    void set(json::value* v);
    void clear();
    val_type type() const;
    bool empty() const;
    class value* detach(bool ignore_undefined = false);

    const json::value* value(bool ignore_undefined = false) const;
    const value_null& as_null() const;
    const value_bool& as_bool() const;
    const value_int& as_int() const;
    const value_float& as_float() const;
    const value_string& as_string() const;
    const value_array& as_array() const;
    const value_object& as_object() const;

    class json::value* value(bool ignore_undefined = false);
    value_null& as_null();
    value_bool& as_bool();
    value_int& as_int();
    value_float& as_float();
    value_string& as_string();
    value_array& as_array();
    value_object& as_object();
};

}

#endif // STRTB_JSON_HOLDER_H
