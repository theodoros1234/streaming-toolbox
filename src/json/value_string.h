#ifndef VALUE_STRING_H
#define VALUE_STRING_H

#include "value.h"
#include <string>

namespace json {

class value_string : public value {
    std::string _value;
public:
    virtual value* copy() const;
    value_string(const char* value);
    value_string(const std::string &value);
    std::string value();
    void set_value(const char* value);
    void set_value(const std::string &value);
};

}

#endif // VALUE_STRING_H
