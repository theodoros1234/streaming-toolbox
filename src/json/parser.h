#ifndef STRTB_JSON_PARSER_H
#define STRTB_JSON_PARSER_H

#include "all_value_types.h"

#include <iostream>
#include <string>

namespace strtb::json::parser {

class invalid_json : public std::exception {
private:
    std::string _what;
public:
    invalid_json(size_t line, size_t col);
    const char* what() const noexcept;
};

json::value* from_stream(std::istream &stream);
json::value* from_string(const std::string &str);
json::value* from_file(const char* path);

}

#endif // STRTB_JSON_PARSER_H
