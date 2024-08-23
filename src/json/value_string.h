#ifndef VALUE_STRING_H
#define VALUE_STRING_H

#include "value.h"
#include <string>

namespace json {

class value_string : public value {
private:
    std::string _value;
public:
    value_string();
    value_string(const char* value);
    value_string(const std::string &value);
    virtual value* copy() const;
    std::string value() const;
    void set_value(const char* value);
    void set_value(const std::string &value);
    virtual void write_to_stream(std::ostream &stream, int pretty_print, int pretty_print_level, const char* newline) const;
};

}

#endif // VALUE_STRING_H
