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

std::vector<std::string> value_object::keys() const {
    std::vector<std::string> copy;
    copy.reserve(_contents.size());
    // Create copy of map's keys and return them
    for (const auto &item : _contents)
        copy.push_back(item.first);
    return copy;
}

size_t value_object::size() const {return _contents.size();}

value& value_object::at(const std::string &key) const {return *_contents.at(key);}

bool value_object::exists(const std::string &key) const {return _contents.find(key) != _contents.end();}

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
    if (!new_val)
        throw std::runtime_error("nullptr was given in arguments");
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

void value_object::set_move(const std::string &key, value* new_val) {
    if (!new_val)
        throw std::runtime_error("nullptr was given in arguments");
    auto itr = _contents.find(key);
    if (itr == _contents.end()) {
        // Create new key with this value
        _contents[key] = new_val;
    } else {
        // Delete existing value and replace it
        delete itr->second;
        itr->second = new_val;
    }
}
void value_object::erase(const std::string &key) {
    auto itr = _contents.find(key);
    if (itr == _contents.end())
        throw std::out_of_range("Key not found");
    delete itr->second;
    _contents.erase(itr);
}

void value_object::write_to_stream(std::ostream &stream, int pretty_print, int pretty_print_level, const char* newline) const {
    stream << '{';
    if (!_contents.empty()) {
        bool first_item = true;
        // Print each key/value pair in the object
        for (const auto &val : _contents) {
            // Comma separator
            if (!first_item)
                stream << ',';
            first_item = false;
            // Newline and space before key/value pair (on pretty print)
            if (pretty_print) {
                stream << newline;
                for (int i=0; i<pretty_print_level + pretty_print; i++)
                    stream << ' ';
            }
            // Key as value_string object (for proper formatting)
            value_string key(val.first);
            key.write_to_stream(stream, pretty_print, pretty_print_level + pretty_print, newline);
            // Colon separator
            stream << ": ";
            // Value
            val.second->write_to_stream(stream, pretty_print, pretty_print_level + pretty_print, newline);
        }
        // Newline and space before end bracket (on pretty print)
        if (pretty_print) {
            stream << newline;
            for (int i=0; i<pretty_print_level; i++)
                stream << ' ';
        }
    }
    stream << '}';
}
