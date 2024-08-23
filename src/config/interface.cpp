#include "interface.h"
#include "../common/strescape.h"

using namespace config;

invalid_category_name::invalid_category_name(const std::string &invalid_name, char invalid_char) {
    _what = "Invalid category name ";
    _what.append(common::string_escape(invalid_name));
    _what.append("; character ");
    _what.append(common::char_escape(invalid_char));
    _what.append(" cannot be part of a file name on at least one supported operating system.");
}

invalid_category_name::invalid_category_name(const std::string &invalid_name) {
    _what = "Invalid category name ";
    _what.append(common::string_escape(invalid_name));
    _what = "; it's a reserved or disallowed file name on at least one supported operating system.";
}

const char* invalid_category_name::what() const noexcept {return _what.c_str();}

category_not_found::category_not_found(const std::string &category_name) {
    _what = "Couldn't find category ";
    _what.append(common::string_escape(category_name));
    _what = "; it isn't loaded or doesn't exist.";
}

const char* category_not_found::what() const noexcept {return _what.c_str();}

category_filesystem_error::category_filesystem_error(const std::string &what) : _what(what) {}

const char* category_filesystem_error::what() const noexcept {return _what.c_str();}

broken_path::broken_path(const path_type &path_until_break) : _path_until_break(path_until_break) {
    _what = "Broken path; the last piece wasn't found or is of wrong type: ";
    bool first = true;
    for (auto &item : _path_until_break) {
        if (!first)
            _what.push_back('>');
        switch (item.type()) {
        case json::VAL_OBJECT:
            _what.append(common::string_escape(item.key()));
            break;
        case json::VAL_ARRAY:
            _what.append(std::to_string(item.pos()));
            break;
        default: // This is here to suppress compiler warnings, and to error out on any unexpected situations that are normally impossible.
            throw std::runtime_error("Reached an unreachable state. This is either a very weird bug, or your hardware is unstable.");
        }
    }
}

broken_path::broken_path(const path_type &path, size_t break_point) {
    // Get path until breaking point
    _path_until_break.reserve(break_point+1);
    for (size_t i=0; i<=break_point && i<path.size(); i++)
        _path_until_break.push_back(path[i]);

    // Message
    _what = "Broken path; the last piece wasn't found or is of wrong type: ";
    bool first = true;
    for (auto &item : _path_until_break) {
        if (!first)
            _what.push_back('>');
        switch (item.type()) {
        case json::VAL_OBJECT:
            _what.append(item.key());
            break;
        case json::VAL_ARRAY:
            _what.append(std::to_string(item.pos()));
            break;
        default: // This is here to suppress compiler warnings, and to error out on any unexpected situations that are normally impossible.
            throw std::runtime_error("Reached an unreachable state. This is either a very weird bug, or your hardware is unstable.");
        }
    }
}

const char* broken_path::what() const noexcept {return _what.c_str();}

const path_type& broken_path::path_until_break() const {return _path_until_break;}

invalid_target::invalid_target(const std::string &action, json::val_type type) {
    _what = "invalid target of type \"";
    switch (type) {
    case json::VAL_NULL:
        _what.append("json::value_null");
        break;
    case json::VAL_BOOL:
        _what.append("json::value_bool");
        break;
    case json::VAL_INT:
        _what.append("json::value_int");
        break;
    case json::VAL_FLOAT:
        _what.append("json::value_float");
        break;
    case json::VAL_STRING:
        _what.append("json::value_string");
        break;
    case json::VAL_ARRAY:
        _what.append("json::value_array");
        break;
    case json::VAL_OBJECT:
        _what.append("json::value_object");
        break;
    }
    _what.append("\" for config action ");
    _what.append(common::string_escape(action));
    _what.append(" with given arguments");
}

invalid_target::invalid_target(const std::string &action) {
    _what = "invalid target for config action ";
    _what.append(common::string_escape(action));
    _what.append("; target was not specified");
}

const char* invalid_target::what() const noexcept {return _what.c_str();}
