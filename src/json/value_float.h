#ifndef VALUE_FLOAT_H
#define VALUE_FLOAT_H

#include "value.h"

namespace json {

class value_float : public value {
private:
    double _value;
public:
    virtual value* copy() const;
    value_float(const double value);
    double value() const;
    void set_value(const double value);
};

}

#endif // VALUE_FLOAT_H
