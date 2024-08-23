#ifndef VALUE_BOOL_H
#define VALUE_BOOL_H

#include "value.h"

namespace json {

class value_bool : public value {
private:
    bool _value;
public:
    value_bool();
    value_bool(const bool value);
    virtual value* copy() const;
    bool value() const;
    void set_value(const bool value);
    virtual void write_to_stream(std::ostream &stream, int pretty_print, int pretty_print_level, const char* newline) const;
};

}

#endif // VALUE_BOOL_H
