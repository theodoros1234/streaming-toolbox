#ifndef STRTB_JSON_VALUE_NULL_H
#define STRTB_JSON_VALUE_NULL_H

#include "value.h"

namespace strtb::json {

class value_null : public value {
public:
    virtual value* copy() const;
    value_null();
    virtual void write_to_stream(std::ostream &stream, int pretty_print, int pretty_print_level, const char* newline) const;
};

}

#endif // STRTB_JSON_VALUE_NULL_H
