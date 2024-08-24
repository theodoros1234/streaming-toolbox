#ifndef VALUE_INT_H
#define VALUE_INT_H

#include "value.h"

namespace strtb::json {

class value_int : public value {
private:
    long long _value;
public:
    value_int();
    value_int(const long long value);
    virtual value* copy() const;
    long long value() const;
    void set_value(const long long value);
    virtual void write_to_stream(std::ostream &stream, int pretty_print, int pretty_print_level, const char* newline) const;
};

}

#endif // VALUE_INT_H
