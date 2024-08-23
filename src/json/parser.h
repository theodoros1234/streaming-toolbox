#ifndef PARSER_H
#define PARSER_H

#include "value.h"
#include "value_null.h"
#include "value_bool.h"
#include "value_int.h"
#include "value_float.h"
#include "value_string.h"
#include "value_array.h"
#include "value_object.h"

#include <iostream>
#include <string>

namespace json::parser {

class invalid_json : public std::exception {
private:
    std::string _what;
public:
    invalid_json(const size_t pos);
    const char* what() const noexcept;
};

json::value* from_stream(std::istream &stream);
json::value* from_string(const std::string &str);
json::value* from_file(const char* path);

}

#endif // PARSER_H
