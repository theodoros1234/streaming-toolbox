#include "value.h"
#include <fstream>
#include <sstream>

using namespace json;

value::value(val_type type) : _type(type) {}

val_type value::type() const {return this->_type;}

void value::write_to_stream(std::ostream &stream, int pretty_print, const char* newline) const {
    write_to_stream(stream, pretty_print, 0, newline);
}

void value::write_to_file(const char *path, int pretty_print, const char* newline) const {
    std::ofstream output_file(path, std::ofstream::out);
    output_file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    write_to_stream(output_file, pretty_print, 0, newline);
}

std::string value::write_to_string(int pretty_print, const char* newline) const {
    std::stringstream str_stream(std::ios_base::out);
    write_to_stream(str_stream, pretty_print, 0, newline);
    return str_stream.str();
}
