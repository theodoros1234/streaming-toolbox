#ifndef VALUE_FLOAT_H
#define VALUE_FLOAT_H

#include "value.h"

namespace strtb::json {

class value_float : public value {
private:
    double _value;
    void deinf();
public:
    value_float();
    value_float(const double value);
    virtual value* copy() const;
    double value() const;
    void set_value(const double value);
    virtual void write_to_stream(std::ostream &stream, int pretty_print, int pretty_print_level, const char* newline) const;
};

}

#endif // VALUE_FLOAT_H
