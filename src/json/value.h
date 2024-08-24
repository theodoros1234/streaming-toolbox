#ifndef VALUE_H
#define VALUE_H

#include <iostream>

namespace strtb::json {

enum val_type {VAL_NULL, VAL_BOOL, VAL_INT, VAL_FLOAT, VAL_STRING, VAL_ARRAY, VAL_OBJECT};

class value {
private:
    val_type _type;
protected:
    value(val_type type);
public:
    virtual value* copy() const = 0;
    virtual ~value() = default;
    val_type type() const;
    virtual void write_to_stream(std::ostream &stream, int pretty_print, int pretty_print_level, const char* newline = "\n") const = 0;
    void write_to_stream(std::ostream &stream, int pretty_print = 0, const char* newline = "\n") const;
    void write_to_file(const char *path, int pretty_print = 0, const char* newline = "\n") const;
    std::string write_to_string(int pretty_print = 0, const char* newline = "\n") const;
};

}

#endif // VALUE_H
