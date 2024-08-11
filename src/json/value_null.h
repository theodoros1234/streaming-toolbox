#ifndef VALUE_NULL_H
#define VALUE_NULL_H

#include "value.h"

namespace json {

class value_null : public value {
public:
    virtual value* copy() const;
    value_null();
};

}

#endif // VALUE_NULL_H
