#include "value_object.h"
#include "../common/strescape.h"
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

void value_object::set(const std::string &key, val_type type) {
    auto itr = _contents.find(key);
    if (itr == _contents.end()) {
        // Create new key with default value
        _contents[key] = value_utils::new_default(type);
    } else {
        // Replace existing value
        value_utils::change_default(&itr->second, type);
    }
}

void value_object::set(const std::string &key, const value_auto &new_val) {
    auto itr = _contents.find(key);
    if (itr == _contents.end()) {
        // Create new key with this value
        _contents[key] = value_utils::new_auto(new_val);
    } else {
        // Replace existing value
        value_utils::change_auto(&itr->second, new_val);
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
            // Key
            stream << common::string_escape(val.first);
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
