#include "value_object.h"
#include "value_null.h"
#include "value_bool.h"
#include "value_int.h"
#include "value_float.h"
#include "value_string.h"
#include <stdexcept>

using namespace json;

value_object::value_object() : json::value(VAL_OBJECT) {}

value_object::value_object(const std::map<std::string, value*> &contents) : json::value(VAL_OBJECT) {set_contents(contents);}

value_object::value_object(const value_object &from) : value_object(from._contents) {}

value_object::~value_object() {
    for (auto item : _contents)
        delete item.second;
}

value* value_object::copy() const {return new value_object(*this);}

std::map<std::string, value*> value_object::contents() const {
    std::map<std::string, value*> copy;
    // Create copy of map's contents and return them
    for (auto &item : _contents)
        copy[item.first] = item.second->copy();
    return copy;
}

void value_object::set_contents(const std::map<std::string, value*> &contents) {
    // Clear existing values first
    clear();
    // Copy things over
    for (auto &item : contents)
        _contents[item.first] = item.second->copy();
}

void value_object::clear() {
    if (!_contents.empty()) {
        for (auto &item : _contents)
            delete item.second;
        _contents.clear();
    }
}

size_t value_object::size() const {return _contents.size();}

value& value_object::at(const std::string &key) const {return *_contents.at(key);}

value* value_object::get(const std::string &key) const {return at(key).copy();}

void value_object::set(const std::string &key) {
    auto itr = _contents.find(key);
    if (itr == _contents.end()) {
        // Create new key with this value
        _contents[key] = new value_null();
    } else {
        // Delete existing value and replace it
        delete itr->second;
        itr->second = new value_null();
    }
}

void value_object::set(const std::string &key, const bool new_val) {
    auto itr = _contents.find(key);
    if (itr == _contents.end()) {
        // Create new key with this value
        _contents[key] = new value_bool(new_val);
    } else {
        // Delete existing value and replace it
        delete itr->second;
        itr->second = new value_bool(new_val);
    }
}

void value_object::set(const std::string &key, const long long new_val) {
    auto itr = _contents.find(key);
    if (itr == _contents.end()) {
        // Create new key with this value
        _contents[key] = new value_int(new_val);
    } else {
        // Delete existing value and replace it
        delete itr->second;
        itr->second = new value_int(new_val);
    }
}

void value_object::set(const std::string &key, const double new_val) {
    auto itr = _contents.find(key);
    if (itr == _contents.end()) {
        // Create new key with this value
        _contents[key] = new value_float(new_val);
    } else {
        // Delete existing value and replace it
        delete itr->second;
        itr->second = new value_float(new_val);
    }
}

void value_object::set(const std::string &key, const char* new_val) {
    auto itr = _contents.find(key);
    if (itr == _contents.end()) {
        // Create new key with this value
        _contents[key] = new value_string(new_val);
    } else {
        // Delete existing value and replace it
        delete itr->second;
        itr->second = new value_string(new_val);
    }
}

void value_object::set(const std::string &key, const std::string &new_val) {
    auto itr = _contents.find(key);
    if (itr == _contents.end()) {
        // Create new key with this value
        _contents[key] = new value_string(new_val);
    } else {
        // Delete existing value and replace it
        delete itr->second;
        itr->second = new value_string(new_val);
    }
}

void value_object::set(const std::string &key, const value* new_val) {
    auto itr = _contents.find(key);
    if (itr == _contents.end()) {
        // Create new key with this value
        _contents[key] = new_val->copy();
    } else {
        // Delete existing value and replace it
        delete itr->second;
        itr->second = new_val->copy();
    }
}

void value_object::erase(const std::string &key) {
    auto itr = _contents.find(key);
    if (itr == _contents.end())
        throw std::out_of_range("Key not found");
    delete itr->second;
    _contents.erase(itr);
}
