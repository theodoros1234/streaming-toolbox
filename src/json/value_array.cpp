#include "value_array.h"
#include "value_null.h"
#include "value_bool.h"
#include "value_int.h"
#include "value_float.h"
#include "value_string.h"
#include <exception>
#include <stdexcept>

using namespace json;

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

void value_array::set(const size_t pos) {
    delete _contents.at(pos);
    _contents.at(pos) = new value_null();
}

void value_array::set(const size_t pos, const bool new_val) {
    delete _contents.at(pos);
    _contents.at(pos) = new value_bool(new_val);
}

void value_array::set(const size_t pos, const long long new_val) {
    delete _contents.at(pos);
    _contents.at(pos) = new value_int(new_val);
}

void value_array::set(const size_t pos, const double new_val) {
    delete _contents.at(pos);
    _contents.at(pos) = new value_float(new_val);
}

void value_array::set(const size_t pos, const char* new_val) {
    delete _contents.at(pos);
    _contents.at(pos) = new value_string(new_val);
}

void value_array::set(const size_t pos, const std::string &new_val) {
    delete _contents.at(pos);
    _contents.at(pos) = new value_string(new_val);
}

void value_array::set(const size_t pos, const value* new_val) {
    delete _contents.at(pos);
    _contents.at(pos) = new_val->copy();
}

value& value_array::at_back() {
    if (_contents.empty())
        throw std::out_of_range("Array is empty");
    return *_contents.back();
}

value* value_array::back() {return at_back().copy();}

void value_array::push_back() {
    value* new_obj = new value_null;
    try {
        _contents.push_back(new_obj);
    } catch (std::exception& e) {
        delete new_obj;
        throw e;
    }
}

void value_array::push_back(const bool new_val) {
    value* new_obj = new value_bool(new_val);
    try {
        _contents.push_back(new_obj);
    } catch (std::exception& e) {
        delete new_obj;
        throw e;
    }
}

void value_array::push_back(const long long new_val) {
    value* new_obj = new value_int(new_val);
    try {
        _contents.push_back(new_obj);
    } catch (std::exception& e) {
        delete new_obj;
        throw e;
    }
}

void value_array::push_back(const double new_val) {
    value* new_obj = new value_float(new_val);
    try {
        _contents.push_back(new_obj);
    } catch (std::exception& e) {
        delete new_obj;
        throw e;
    }
}

void value_array::push_back(const char* new_val) {
    value* new_obj = new value_string(new_val);
    try {
        _contents.push_back(new_obj);
    } catch (std::exception& e) {
        delete new_obj;
        throw e;
    }
}

void value_array::push_back(const std::string &new_val) {
    value* new_obj = new value_string(new_val);
    try {
        _contents.push_back(new_obj);
    } catch (std::exception& e) {
        delete new_obj;
        throw e;
    }
}

void value_array::push_back(const value* new_val) {
    value* new_obj = new_val->copy();
    try {
        _contents.push_back(new_obj);
    } catch (std::exception& e) {
        delete new_obj;
        throw e;
    }
}

void value_array::pop_back() {
    if (_contents.empty())
        throw std::out_of_range("Array is empty");
    delete _contents.back();
    _contents.pop_back();
}

void value_array::insert(const size_t pos) {
    if (pos > _contents.size())
        throw std::out_of_range("Out of range");
    value* new_obj = new value_null();
    try {
        _contents.insert(_contents.begin()+pos, new_obj);
    } catch (std::exception& e) {
        delete new_obj;
        throw e;
    }
}

void value_array::insert(const size_t pos, const bool new_val) {
    if (pos > _contents.size())
        throw std::out_of_range("Out of range");
    value* new_obj = new value_bool(new_val);
    try {
        _contents.insert(_contents.begin()+pos, new_obj);
    } catch (std::exception& e) {
        delete new_obj;
        throw e;
    }
}

void value_array::insert(const size_t pos, const long long new_val) {
    if (pos > _contents.size())
        throw std::out_of_range("Out of range");
    value* new_obj = new value_int(new_val);
    try {
        _contents.insert(_contents.begin()+pos, new_obj);
    } catch (std::exception& e) {
        delete new_obj;
        throw e;
    }
}

void value_array::insert(const size_t pos, const double new_val) {
    if (pos > _contents.size())
        throw std::out_of_range("Out of range");
    value* new_obj = new value_float(new_val);
    try {
        _contents.insert(_contents.begin()+pos, new_obj);
    } catch (std::exception& e) {
        delete new_obj;
        throw e;
    }
}

void value_array::insert(const size_t pos, const char* new_val) {
    if (pos > _contents.size())
        throw std::out_of_range("Out of range");
    value* new_obj = new value_string(new_val);
    try {
        _contents.insert(_contents.begin()+pos, new_obj);
    } catch (std::exception& e) {
        delete new_obj;
        throw e;
    }
}

void value_array::insert(const size_t pos, const std::string &new_val) {
    if (pos > _contents.size())
        throw std::out_of_range("Out of range");
    value* new_obj = new value_string(new_val);
    try {
        _contents.insert(_contents.begin()+pos, new_obj);
    } catch (std::exception& e) {
        delete new_obj;
        throw e;
    }
}

void value_array::insert(const size_t pos, const value* new_val) {
    if (pos > _contents.size())
        throw std::out_of_range("Out of range");
    value* new_obj = new_val->copy();
    try {
        _contents.insert(_contents.begin()+pos, new_obj);
    } catch (std::exception& e) {
        delete new_obj;
        throw e;
    }
}

void value_array::erase(const size_t pos) {
    if (pos >= _contents.size())
        throw std::out_of_range("Out of range");
    delete _contents.at(pos);
    _contents.erase(_contents.begin()+pos);
}
