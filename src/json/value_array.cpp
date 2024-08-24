#include "value_array.h"
#include "value_null.h"
#include "value_bool.h"
#include "value_int.h"
#include "value_float.h"
#include "value_string.h"
#include <stdexcept>

using namespace strtb;
using namespace strtb::json;

value_array::value_array() : json::value(VAL_ARRAY) {}

value_array::value_array(const std::vector<value*> &contents) : json::value(VAL_ARRAY) {set_contents(contents);}

value_array::value_array(const value_array &from) : value_array(from._contents) {};

value_array::~value_array() {
    for (auto v : _contents)
        delete v;
}

std::vector<value*> value_array::contents() const {
    std::vector<value*> copy;
    // Create copy of array's contents and return them
    copy.reserve(_contents.size());
    for (auto item : _contents)
        copy.push_back(item->copy());
    return copy;
}

void value_array::set_contents(const std::vector<value*> &contents) {
    // Clear existing values first
    this->clear();
    // Copy new values over
    _contents.reserve(contents.size());
    for (auto item : contents)
        _contents.push_back(item->copy());
}

void value_array::clear() {
    if (!_contents.empty()) {
        for (auto item : _contents)
            delete item;
        _contents.clear();
    }
}

value* value_array::copy() const {return new value_array(*this);}

size_t value_array::size() const {return _contents.size();}

value& value_array::at(const size_t pos) const {return *_contents.at(pos);}

value* value_array::get(const size_t pos) const {return at(pos).copy();}

void value_array::set(const size_t pos, val_type type) {
    value_utils::change_default(&_contents.at(pos), type);
}

void value_array::set(const size_t pos, const value_auto &new_val) {
    value_utils::change_auto(&_contents.at(pos), new_val);
}

void value_array::set_move(const size_t pos, value* new_val) {
    if (!new_val)
        throw std::runtime_error("nullptr was given in arguments");
    delete _contents.at(pos);
    _contents.at(pos) = new_val;
}

value& value_array::at_back() {
    if (_contents.empty())
        throw std::out_of_range("Array is empty");
    return *_contents.back();
}

value* value_array::back() {return at_back().copy();}

void value_array::push_back(val_type type) {
    value* new_obj = value_utils::new_default(type);
    try {
        _contents.push_back(new_obj);
    } catch (...) {
        delete new_obj;
        throw;
    }
}

void value_array::push_back(const value_auto &new_val) {
    value* new_obj = value_utils::new_auto(new_val);
    try {
        _contents.push_back(new_obj);
    } catch (...) {
        delete new_obj;
        throw;
    }
}

void value_array::push_back_move(value* new_val) {
    if (!new_val)
        throw std::runtime_error("nullptr was given in arguments");
    _contents.push_back(new_val);
}

void value_array::pop_back() {
    if (_contents.empty())
        throw std::out_of_range("Array is empty");
    delete _contents.back();
    _contents.pop_back();
}

void value_array::insert(const size_t pos, val_type type) {
    if (pos > _contents.size())
        throw std::out_of_range("Out of range");
    value* new_obj = value_utils::new_default(type);
    try {
        _contents.insert(_contents.begin()+pos, new_obj);
    } catch (...) {
        delete new_obj;
        throw;
    }
}

void value_array::insert(const size_t pos, const value_auto &new_val) {
    if (pos > _contents.size())
        throw std::out_of_range("Out of range");
    value* new_obj = value_utils::new_auto(new_val);
    try {
        _contents.insert(_contents.begin()+pos, new_obj);
    } catch (...) {
        delete new_obj;
        throw;
    }
}

void value_array::insert_move(const size_t pos, value* new_val) {
    if (pos > _contents.size())
        throw std::out_of_range("Out of range");
    if (!new_val)
        throw std::runtime_error("nullptr was given in arguments");
    _contents.insert(_contents.begin()+pos, new_val);
}

void value_array::erase(const size_t pos) {
    if (pos >= _contents.size())
        throw std::out_of_range("Out of range");
    delete _contents.at(pos);
    _contents.erase(_contents.begin()+pos);
}

void value_array::write_to_stream(std::ostream &stream, int pretty_print, int pretty_print_level, const char* newline) const {
    stream << '[';
    if (!_contents.empty()) {
        bool first_item = true;
        // Print each item in the array
        for (const auto &val : _contents) {
            // Comma separator
            if (!first_item)
                stream << ',';
            first_item = false;
            // Newline and space before value (on pretty print)
            if (pretty_print) {
                stream << newline;
                for (int i=0; i<pretty_print_level + pretty_print; i++)
                    stream << ' ';
            }
            // The value itself
            val->write_to_stream(stream, pretty_print, pretty_print_level + pretty_print, newline);
        }
        // Newline and space before end bracket (on pretty print)
        if (pretty_print) {
            stream << newline;
            for (int i=0; i<pretty_print_level; i++)
                stream << ' ';
        }
    }
    stream << ']';
}
