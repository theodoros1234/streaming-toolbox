#ifndef SYSTEM_H
#define SYSTEM_H

#include "interface.h"
#include "../json/value.h"
#include "../logging/logging.h"

#include <string>
#include <map>
#include <mutex>

namespace strtb::config {

class system : public interface {
private:
    struct category_instance {
        std::filesystem::path path;
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
    virtual void load_category(const std::string &category_name);
    virtual void save_category(const std::string &category_name, bool force = false);
    virtual void close_category(const std::string &category_name, bool save_if_needed = true);

    // Category roots
    virtual json::value* get_category_root(const std::string &category_name);
    virtual json::val_type get_category_root_type(const std::string &category_name);
    virtual void set_category_root(const std::string &category_name, const json::val_type type);
    virtual void set_category_root(const std::string &category_name, const json::value_auto &value);
    virtual void set_category_root_move(const std::string &category_name, json::value *value);
    virtual void clear_category_root(const std::string &category_name);

    // Values
    virtual bool exists(const std::string &category_name, const path_type &path);
    virtual json::val_type get_type(const std::string &category_name, const path_type &path);
    virtual json::value* get_value(const std::string &category_name, const path_type &path);
    virtual void set_value(const std::string &category_name, const path_type &path, json::val_type type);
    virtual void set_value(const std::string &category_name, const path_type &path, const json::value_auto &value);
    virtual void set_value_move(const std::string &category_name, const path_type &path, json::value *value);
    virtual void erase_value(const std::string &category_name, const path_type &path);
    virtual void write_to_stream(const std::string &category_name, const path_type &path, std::ostream &stream, int pretty_print = 0, const char* newline = "\n");
    virtual std::string write_to_string(const std::string &category_name, const path_type &path, int pretty_print = 0, const char* newline = "\n");

    // Arrays and objects
    virtual size_t size(const std::string &category_name, const path_type &path);
    virtual void clear(const std::string &category_name, const path_type &path);

    // Array-specific
    virtual json::value* array_get_back(const std::string &category_name, const path_type &path);
    virtual void array_push_back(const std::string &category_name, const path_type &path, json::val_type type);
    virtual void array_push_back(const std::string &category_name, const path_type &path, const json::value_auto &value);
    virtual void array_push_back_move(const std::string &category_name, const path_type &path, json::value *value);
    virtual void array_pop_back(const std::string &category_name, const path_type &path);
    virtual json::value* array_pop_and_get_back(const std::string &category_name, const path_type &path);
    virtual void array_insert(const std::string &category_name, const path_type &path, size_t pos, json::val_type type);
    virtual void array_insert(const std::string &category_name, const path_type &path, size_t pos, const json::value_auto &value);
    virtual void array_insert_move(const std::string &category_name, const path_type &path, size_t pos, json::value *value);
};

}

#endif // SYSTEM_H
