#ifndef STRTB_JSON_VALUE_H
#define STRTB_JSON_VALUE_H

#include <iostream>
#include <exception>
#include <string>
#include <vector>
#include <map>

namespace strtb::json {

enum val_type {VAL_NULL, VAL_BOOL, VAL_INT, VAL_FLOAT, VAL_STRING, VAL_ARRAY, VAL_OBJECT, VAL_UNDEFINED};
std::string val_type_str(val_type type);

class json_error : public std::exception {};

class invalid_type : public json_error {
public:
    const char* what() const noexcept;
};

class undefined_exception : public json_error {
public:
    const char* what() const noexcept;
};

class value {
private:
    val_type _type;
protected:
    value(val_type type);
public:
    virtual value* copy() const = 0;
    virtual ~value() = default;
    val_type type() const;
    virtual void write_to_stream(std::ostream &stream, int pretty_print, int pretty_print_level, const char* newline = "\n") const = 0;
    void write_to_stream(std::ostream &stream, int pretty_print = 0, const char* newline = "\n") const;
    void write_to_file(const char *path, int pretty_print = 0, const char* newline = "\n") const;
    std::string write_to_string(int pretty_print = 0, const char* newline = "\n") const;
};

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

value* new_default(val_type type);
value* new_auto(const value_auto &val);
void change_default(value** old_val, val_type type);
void change_auto(value** old_val, const value_auto &new_val);

val_type type_from_string(const std::string& str);
std::string type_to_string(val_type type);

}

#endif // STRTB_JSON_VALUE_H
