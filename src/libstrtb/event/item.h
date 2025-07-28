#ifndef STRTB_EVENT_ITEM_H
#define STRTB_EVENT_ITEM_H

#include <string>
#include <vector>
#include "../json/value_object.h"
#include "../logging/logging.h"
#include <stdint.h>

namespace strtb::event {

class event_exception : public std::exception {
private:
    std::string _what;
public:
    event_exception(const std::string& what);
    const char* what() const noexcept;
};

class internal_error : public event_exception {
public:
    internal_error(const std::string& what);
    internal_error(const std::string& what, logging::source& log_to);
};

class not_found : public event_exception {
public:
    not_found(const std::string& what);
};

class wrong_type : public event_exception {
public:
    wrong_type(const std::string& what);
};

class out_of_scope : public event_exception {
public:
    out_of_scope(const std::string& what);
};

class already_exists : public event_exception {
public:
    already_exists(const std::string& what);
};

class parsing_error : public event_exception {
public:
    parsing_error(const std::string& what);
};

class invalid_path : public event_exception {
public:
    invalid_path(const std::string& what, ssize_t pos);
    const ssize_t pos;
};

class bad_definition : public event_exception {
public:
    bad_definition(const std::string& what);
};

struct param_definition {
    std::string name, description;
    json::val_type type = json::VAL_UNDEFINED;
    bool required = false;
    param_definition* array_definition = nullptr;
    std::vector<param_definition*> object_definition;

    param_definition() = default;
    param_definition(const param_definition& from);
    param_definition(param_definition&& from);
    param_definition(json::val_type type);
    param_definition(const std::string& name, const std::string& description, json::val_type type, bool required);
    param_definition(const json::value_object* from);
    ~param_definition();
    param_definition& operator=(const param_definition& from);
    param_definition& operator=(param_definition&& from);
    void array_define(json::val_type type);
    void array_clear_definition();
    void object_add_definition(const std::string& name, const std::string& description, json::val_type type, bool required);
    void object_clear_definitions();
};

struct example_definition {
    json::value_object params, returns;

    example_definition() = default;
    example_definition(const example_definition& other);
    example_definition(example_definition&& other);
    example_definition(const json::value_object* params, const json::value_object* returns);
    example_definition(const json::value_object& params, const json::value_object& returns);
    example_definition(json::value_object&& params, json::value_object&& returns);
    example_definition& operator=(const example_definition& other);
    example_definition& operator=(example_definition&& other);
};

enum item_type {ITEM_UNDEFINED, ITEM_CATEGORY, ITEM_EVENT_SRC, ITEM_ACTION_SINK};

typedef std::vector<std::string> item_path;

bool item_path_validate_segment(const std::string& segment);
ssize_t item_path_validate(const item_path& path);
std::string item_path_to_string(const item_path& path);
item_path to_item_path(const std::string& path_str, size_t max_segment_length = 256, size_t max_depth = 64);

struct item_ref {
    uint64_t resource_id = 0;
    item_path path;
};

struct item_info {
    uint64_t provider_id = 0;
    item_type type = ITEM_UNDEFINED;
    std::string display_name, description;
    std::vector<param_definition> params, returns;
    std::vector<example_definition> examples;

    item_info() = default;
    item_info(item_type type, const std::string& display_name, const std::string& description);
    item_info(const json::value_object* from);
};

struct item_listing : public item_info {
    uint64_t resource_id = 0;
    std::string name;
};

}

#endif // STRTB_EVENT_ITEM_H
