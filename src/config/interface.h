#ifndef INTERFACE_H
#define INTERFACE_H

#include "id_type.h"
#include "../json/value.h"
#include "../json/value_utils.h"

#include <string>
#include <vector>
#include <exception>

namespace strtb::config {

typedef std::vector<id_type> path_type;

class invalid_category_name : public std::exception {
private:
    std::string _what;
public:
    invalid_category_name(const std::string &invalid_name, char invalid_char);
    invalid_category_name(const std::string &invalid_name);
    const char* what() const noexcept;
};

class category_not_found : public std::exception {
private:
    std::string _what;
public:
    category_not_found(const std::string &category_name);
    const char* what() const noexcept;
};

class category_filesystem_error : public std::exception {
private:
    std::string _what;
public:
    category_filesystem_error(const std::string &what);
    const char* what() const noexcept;
};

class broken_path : public std::exception {
private:
    std::string _what;
    std::string _category_name;
    path_type _path_until_break;
public:
    broken_path(const std::string &category_name, const path_type &path_until_break);
    broken_path(const std::string &category_name, const path_type &path, size_t break_point);
    const char* what() const noexcept;
    const std::string& category_name() const;
    const path_type& path_until_break() const;
};

class invalid_target : public std::exception {
private:
    std::string _what;
public:
    invalid_target(const std::string &action, json::val_type type);
    invalid_target(const std::string &action);
    const char* what() const noexcept;
};

class interface {
protected:
    virtual ~interface() = default;
public:
    // Categories
    virtual void load_category(const std::string &category_name) = 0;
    virtual void save_category(const std::string &category_name, bool force = false) = 0;
    virtual void close_category(const std::string &category_name, bool save_if_needed = true) = 0;

    // Category roots
    virtual json::value* get_category_root(const std::string &category_name) = 0;
    virtual json::val_type get_category_root_type(const std::string &category_name) = 0;
    virtual void set_category_root(const std::string &category_name, const json::val_type type) = 0;
    virtual void set_category_root(const std::string &category_name, const json::value_auto &value) = 0;
    virtual void set_category_root_move(const std::string &category_name, json::value *value) = 0;
    virtual void clear_category_root(const std::string &category_name) = 0;

    // Values
    virtual bool exists(const std::string &category_name, const path_type &path) = 0;
    virtual json::val_type get_type(const std::string &category_name, const path_type &path) = 0;
    virtual json::value* get_value(const std::string &category_name, const path_type &path) = 0;
    virtual void set_value(const std::string &category_name, const path_type &path, json::val_type type) = 0;
    virtual void set_value(const std::string &category_name, const path_type &path, const json::value_auto &value) = 0;
    virtual void set_value_move(const std::string &category_name, const path_type &path, json::value *value) = 0;
    virtual void erase_value(const std::string &category_name, const path_type &path) = 0;
    virtual void write_to_stream(const std::string &category_name, const path_type &path, std::ostream &stream, int pretty_print = 0, const char* newline = "\n") = 0;
    virtual std::string write_to_string(const std::string &category_name, const path_type &path, int pretty_print = 0, const char* newline = "\n") = 0;

    // Arrays and objects
    virtual size_t size(const std::string &category_name, const path_type &path) = 0;
    virtual void clear(const std::string &category_name, const path_type &path) = 0;

    // Array-specific
    virtual json::value* array_get_back(const std::string &category_name, const path_type &path) = 0;
    virtual void array_push_back(const std::string &category_name, const path_type &path, json::val_type type) = 0;
    virtual void array_push_back(const std::string &category_name, const path_type &path, const json::value_auto &value) = 0;
    virtual void array_push_back_move(const std::string &category_name, const path_type &path, json::value *value) = 0;
    virtual void array_pop_back(const std::string &category_name, const path_type &path) = 0;
    virtual json::value* array_pop_and_get_back(const std::string &category_name, const path_type &path) = 0;
    virtual void array_insert(const std::string &category_name, const path_type &path, size_t pos, json::val_type type) = 0;
    virtual void array_insert(const std::string &category_name, const path_type &path, size_t pos, const json::value_auto &value) = 0;
    virtual void array_insert_move(const std::string &category_name, const path_type &path, size_t pos, json::value *value) = 0;
};

}

#endif // INTERFACE_H
