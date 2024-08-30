#ifndef STRTB_JSON_VALUE_UTILS_H
#define STRTB_JSON_VALUE_UTILS_H

#include "value.h"

#include <string>
#include <vector>
#include <map>

namespace strtb::json {

class value_auto {
private:
    val_type _type;
    bool _is_ptr = false;
    bool _is_c_str = false;
    union {
        bool b;
        long long i;
        double f;
        const char* c_str;
        const std::string* str;
        const std::vector<value*>* arr;
        const std::map<std::string, value*>* obj;
        const value* v;
    } _value;
public:
    value_auto(bool);
    value_auto(int);
    value_auto(long);
    value_auto(long long);
    value_auto(unsigned int);
    value_auto(unsigned long);
    value_auto(unsigned long long);
    value_auto(double);
    value_auto(const char*);
    value_auto(const std::string&);
    value_auto(const std::vector<value*>&);
    value_auto(const std::map<std::string, value*>&);
    value_auto(const value*);
    val_type type() const;
    bool is_ptr() const;
    bool is_c_str() const;
    bool value_as_bool() const;
    long long value_as_int() const;
    double value_as_float() const;
    const char* value_as_c_str() const;
    const std::string& value_as_string() const;
    const std::vector<value*>& value_as_array() const;
    const std::map<std::string, value*>& value_as_object() const;
    const value* value_as_ptr() const;
};

namespace value_utils {

value* new_default(val_type type);
value* new_auto(const value_auto &val);
void change_default(value** old_val, val_type type);
void change_auto(value** old_val, const value_auto &new_val);

}

}

#endif // STRTB_JSON_VALUE_UTILS_H
