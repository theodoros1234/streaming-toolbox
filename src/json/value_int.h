#ifndef VALUE_INT_H
#define VALUE_INT_H

#include "value.h"

namespace json {

class value_int : public value {
private:
    long long _value;
public:
    virtual value* copy() const;
    value_int(const long long value);
    long long value() const;
    void set_value(const long long value);
};

}

#endif // VALUE_INT_H
