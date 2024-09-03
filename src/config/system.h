#ifndef STRTB_CONFIG_SYSTEM_H
#define STRTB_CONFIG_SYSTEM_H

#include "id_type.h"
#include "../json/value.h"
#include "../json/value_utils.h"
#include "../logging/logging.h"

#include <string>
#include <map>
#include <mutex>

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

class system {
private:
    struct category_instance {
        std::filesystem::path path, path_bak;
        json::value* root;
        bool changed = false;
    };
    std::mutex _lock;
    logging::source log;
    std::filesystem::path _config_dir;
    std::map<std::string, category_instance> _categories;

    static std::string to_lowercase(const std::string &category_name);
    category_instance& find_category(const std::string &category_name, bool changed = false);
    static json::value* find_next_item(json::value* current, const id_type &go_to);
    json::value* follow_path(const std::string &category_name, const path_type &path, bool skip_last = false);
public:
    system(const std::filesystem::path &config_dir);
    virtual ~system();

    // Categories
    void load_category(const std::string &category_name);
    void save_category(const std::string &category_name, bool force = false);
    void close_category(const std::string &category_name, bool save_if_needed = true);

    // Category roots
    json::value* get_category_root(const std::string &category_name);
    json::val_type get_category_root_type(const std::string &category_name);
    void set_category_root(const std::string &category_name, const json::val_type type);
    void set_category_root(const std::string &category_name, const json::value_auto &value);
    void set_category_root_move(const std::string &category_name, json::value *value);
    void clear_category_root(const std::string &category_name);

    // Values
    bool exists(const std::string &category_name, const path_type &path);
    json::val_type get_type(const std::string &category_name, const path_type &path);
    json::value* get_value(const std::string &category_name, const path_type &path);
    void set_value(const std::string &category_name, const path_type &path, json::val_type type);
    void set_value(const std::string &category_name, const path_type &path, const json::value_auto &value);
    void set_value_move(const std::string &category_name, const path_type &path, json::value *value);
    void erase_value(const std::string &category_name, const path_type &path);
    void write_to_stream(const std::string &category_name, const path_type &path, std::ostream &stream, int pretty_print = 0, const char* newline = "\n");
    std::string write_to_string(const std::string &category_name, const path_type &path, int pretty_print = 0, const char* newline = "\n");

    // Arrays and objects
    size_t size(const std::string &category_name, const path_type &path);
    void clear(const std::string &category_name, const path_type &path);

    // Array-specific
    json::value* array_get_back(const std::string &category_name, const path_type &path);
    void array_push_back(const std::string &category_name, const path_type &path, json::val_type type);
    void array_push_back(const std::string &category_name, const path_type &path, const json::value_auto &value);
    void array_push_back_move(const std::string &category_name, const path_type &path, json::value *value);
    void array_pop_back(const std::string &category_name, const path_type &path);
    json::value* array_pop_and_get_back(const std::string &category_name, const path_type &path);
    void array_insert(const std::string &category_name, const path_type &path, size_t pos, json::val_type type);
    void array_insert(const std::string &category_name, const path_type &path, size_t pos, const json::value_auto &value);
    void array_insert_move(const std::string &category_name, const path_type &path, size_t pos, json::value *value);
};

extern system *main;

}

#endif // STRTB_CONFIG_SYSTEM_H
