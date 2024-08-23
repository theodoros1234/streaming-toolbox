#include "value_utils.h"
#include "value_null.h"
#include "value_bool.h"
#include "value_int.h"
#include "value_float.h"
#include "value_string.h"
#include "value_array.h"
#include "value_object.h"

#include <stdexcept>

using namespace json;
using namespace json::value_utils;

value_auto::value_auto(bool v) : _type(VAL_BOOL) {_value.b = v;}
value_auto::value_auto(int v) : _type(VAL_INT) {_value.i = v;}
value_auto::value_auto(long v) : _type(VAL_INT) {_value.i = v;}
value_auto::value_auto(long long v) : _type(VAL_INT) {_value.i = v;}
value_auto::value_auto(unsigned int v) : _type(VAL_INT) {_value.i = v;}
value_auto::value_auto(unsigned long v) : _type(VAL_INT) {_value.i = v;}
value_auto::value_auto(unsigned long long v) : _type(VAL_INT) {_value.i = v;}
value_auto::value_auto(double v) : _type(VAL_FLOAT) {_value.f = v;}
value_auto::value_auto(const char* v) : _type(VAL_STRING), _is_c_str(true) {_value.c_str = v;}
value_auto::value_auto(const std::string &v) : _type(VAL_STRING) {_value.str = &v;}
value_auto::value_auto(const std::vector<value*> &v) : _type(VAL_ARRAY) {_value.arr = &v;}
value_auto::value_auto(const std::map<std::string, value*> &v) : _type(VAL_OBJECT) {_value.obj = &v;}
value_auto::value_auto(const value* v) : _is_ptr(true) {_value.v = v;}
val_type value_auto::type() const {return _type;}
bool value_auto::is_ptr() const {return _is_ptr;}
bool value_auto::is_c_str() const {return _is_c_str;}
bool value_auto::value_as_bool() const {return _value.b;}
long long value_auto::value_as_int() const {return _value.i;}
double value_auto::value_as_float() const {return _value.f;}
const char* value_auto::value_as_c_str() const {return _value.c_str;}
const std::string& value_auto::value_as_string() const {return *_value.str;}
const std::vector<value*>& value_auto::value_as_array() const {return *_value.arr;}
const std::map<std::string, value*>& value_auto::value_as_object() const {return *_value.obj;}
const value* value_auto::value_as_ptr() const {return _value.v;}

value* value_utils::new_default(val_type type) {
    switch (type) {
    case VAL_NULL:
        return new value_null();
        break;
    case VAL_BOOL:
        return new value_bool();
        break;
    case VAL_INT:
        return new value_int();
        break;
    case VAL_FLOAT:
        return new value_float();
        break;
    case VAL_STRING:
        return new value_string();
        break;
    case VAL_ARRAY:
        return new value_array();
        break;
    case VAL_OBJECT:
        return new value_object();
        break;
    default: // This is here to suppress compiler warnings, and to error out on any unexpected situations that are normally impossible.
        throw std::runtime_error("Reached an unreachable state. This is either a very weird bug, or your hardware is unstable.");
    }
}

value* value_utils::new_auto(const value_auto &val) {
    if (val.is_ptr()) {
        if (!val.value_as_ptr())
            throw std::runtime_error("nullptr was given in arguments");
        return val.value_as_ptr()->copy();
    } else switch (val.type()) {
    case VAL_BOOL:
        return new value_bool(val.value_as_bool());
    case VAL_INT:
        return new value_int(val.value_as_int());
    case VAL_FLOAT:
        return new value_float(val.value_as_float());
    case VAL_STRING:
        if (val.is_c_str()) {
            if (!val.value_as_c_str())
                throw std::runtime_error("nullptr was given in arguments");
            return new value_string(val.value_as_c_str());
        } else
            return new value_string(val.value_as_string());
    case VAL_ARRAY:
        return new value_array(val.value_as_array());
    case VAL_OBJECT:
        return new value_object(val.value_as_object());
    default: // This is here to suppress compiler warnings, and to error out on any unexpected situations that are normally impossible.
        throw std::runtime_error("Reached an unreachable state. This is either a very weird bug, or your hardware is unstable.");
    }
}

void value_utils::change_default(value** old_val, val_type type) {
    val_type old_type = (*old_val)->type();
    switch (type) {
    case VAL_NULL:
        if (old_type != VAL_NULL) {
            delete *old_val;
            *old_val = new value_null();
        }
        break;

    case VAL_BOOL:
        if (old_type == VAL_BOOL)
            ((value_bool*)*old_val)->set_value(false);
        else {
            delete *old_val;
            *old_val = new value_bool();
        }
        break;

    case VAL_INT:
        if (old_type == VAL_INT)
            ((value_int*)*old_val)->set_value(0);
        else {
            delete *old_val;
            *old_val = new value_int();
        }
        break;

    case VAL_FLOAT:
        if (old_type == VAL_FLOAT)
            ((value_float*)*old_val)->set_value(0);
        else {
            delete *old_val;
            *old_val = new value_float();
        }
        break;

    case VAL_STRING:
        if (old_type == VAL_STRING)
            ((value_string*)*old_val)->set_value("");
        else {
            delete *old_val;
            *old_val = new value_string();
        }
        break;

    case VAL_ARRAY:
        if (old_type == VAL_ARRAY)
            ((value_array*)*old_val)->clear();
        else {
            delete *old_val;
            *old_val = new value_array();
        }
        break;

    case VAL_OBJECT:
        if (old_type == VAL_OBJECT)
            ((value_object*)*old_val)->clear();
        else {
            delete *old_val;
            *old_val = new value_object();
        }
        break;
    }
}

void value_utils::change_auto(value** old_val, const value_auto &new_val) {
    val_type old_type = (*old_val)->type();

    if (new_val.is_ptr()) {
        if (!new_val.value_as_ptr())
            throw std::runtime_error("nullptr was given in arguments");
        delete *old_val;
        *old_val = new_val.value_as_ptr()->copy();
    } else switch (new_val.type()) {

    case VAL_BOOL:
        if (old_type == VAL_BOOL)
            ((value_bool*)*old_val)->set_value(new_val.value_as_bool());
        else {
            delete *old_val;
            *old_val = new value_bool(new_val.value_as_bool());
        }
        break;

    case VAL_INT:
        if (old_type == VAL_INT)
            ((value_int*)*old_val)->set_value(new_val.value_as_int());
        else {
            delete *old_val;
            *old_val = new value_int(new_val.value_as_int());
        }
        break;

    case VAL_FLOAT:
        if (old_type == VAL_FLOAT)
            ((value_float*)*old_val)->set_value(new_val.value_as_float());
        else {
            delete *old_val;
            *old_val = new value_float(new_val.value_as_float());
        }
        break;

    case VAL_STRING:
        if (new_val.is_c_str()) {
            if (!new_val.value_as_c_str())
                throw std::runtime_error("nullptr was given in arguments");
            if (old_type == VAL_STRING)
                ((value_string*)*old_val)->set_value(new_val.value_as_c_str());
            else {
                delete *old_val;
                *old_val = new value_string(new_val.value_as_c_str());
            }
        } else {
            if (old_type == VAL_STRING)
                ((value_string*)*old_val)->set_value(new_val.value_as_string());
            else {
                delete *old_val;
                *old_val = new value_string(new_val.value_as_string());
            }
        }
        break;

    case VAL_ARRAY:
        if (old_type == VAL_ARRAY)
            ((value_array*)*old_val)->set_contents(new_val.value_as_array());
        else {
            delete *old_val;
            *old_val = new value_array(new_val.value_as_array());
        }
        break;

    case VAL_OBJECT:
        if (old_type == VAL_OBJECT)
            ((value_object*)*old_val)->set_contents(new_val.value_as_object());
        else {
            delete *old_val;
            *old_val = new value_object(new_val.value_as_object());
        }
        break;

    default: // This is here to suppress compiler warnings, and to error out on any unexpected situations that are normally impossible.
        throw std::runtime_error("Reached an unreachable state. This is either a very weird bug, or your hardware is unstable.");
    }
}
