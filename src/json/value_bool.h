#ifndef VALUE_BOOL_H
#define VALUE_BOOL_H

#include "value.h"

namespace json {

class value_bool : public value {
private:
    bool _value;
public:
    virtual value* copy() const;
    value_bool(const bool value);
    bool value() const;
    void set_value(const bool value);
    virtual void write_to_stream(std::ostream &stream, int pretty_print, int pretty_print_level, const char* newline) const;
};

}

#endif // VALUE_BOOL_H
