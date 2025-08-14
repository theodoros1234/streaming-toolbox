#ifndef STRTB_EVENT_ITEM_H
#define STRTB_EVENT_ITEM_H

#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <stdint.h>
#include "../json/value_object.h"
#include "../logging/logging.h"
#include "../json/holder.h"

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
    internal_error(const std::string& what, logging::source& log_to, const char* file, int line, const char* func);
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

class bad_state : public event_exception {
public:
    bad_state(const std::string& what);
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
    void set(const std::string& name, const std::string& description, json::val_type type, bool required);
    void array_define(json::val_type type);
    void array_clear_definition();
    void object_add_definition(const std::string& name, const std::string& description, json::val_type type, bool required);
    void object_clear_definitions();
};

struct example_definition {
    json::holder params, returns;

    example_definition() = default;
    example_definition(const example_definition& other);
    example_definition(example_definition&& other);
    example_definition(const json::value* params, const json::value* returns);
    example_definition& operator=(const example_definition& other);
    example_definition& operator=(example_definition&& other);
};

enum item_type {ITEM_UNDEFINED, ITEM_CATEGORY, ITEM_EVENT_SRC, ITEM_ACTION_SINK};

const char* item_type_to_string(item_type type);

class item_path : public std::vector<std::string> {
    using std::vector<std::string>::vector;
public:
    item_path(const std::string& path, size_t max_segment_length = 256, size_t max_depth = 64);
    item_path(const char* path, size_t max_segment_length = 256, size_t max_depth = 64);
    std::string to_string() const;
    ssize_t validate() const;
    void validate_with_exception() const;
    bool validate_segment(size_t pos) const;
    static bool validate_segment(const std::string& segment);
};

struct item_ref {
    uint64_t resource_id = 0;
    item_path path;
};

struct item_info {
    uint64_t provider_id = 0;
    item_type type = ITEM_UNDEFINED;
    std::string display_name, description;
    std::vector<param_definition> params;
    param_definition returns;
    std::vector<example_definition> examples;

    item_info() = default;
    item_info(item_type type, const std::string& display_name, const std::string& description);
    item_info(const json::value_object* from);
};

struct item_listing : public item_info {
    uint64_t resource_id = 0;
    std::string name;
};

enum path_follower_status {
    PATH_FL_UNDEFINED,  // must be changed immediately after creation by the event system
    PATH_FL_READY,      // resource is attached to its wanted target
    PATH_FL_WAITING,    // resource is attached to a category waiting for its wanted target
    PATH_FL_WRONG_TYPE, // resource cannot attach to its wanted target cause it was a different item type
    PATH_FL_BAD_PARAM   // resource cannot attach to its wanted target due to having wrong parameters
};

struct item_info_path_follower {
    std::string owner_name;
    uint64_t follower_rid = 0;
    item_path path;
    item_type wanted_type = ITEM_UNDEFINED;
    path_follower_status status = PATH_FL_UNDEFINED;
    std::string diagnostic_info;
};

struct item_info_event_sub {
    std::string listener_name;
    uint64_t event_sub_rid = 0;
    json::holder param;
};

struct item_info_action_requester {
    std::string owner_name;
    uint64_t follower_rid = 0;
};

struct item_info_action_sink {
    std::vector<item_info_action_requester> requesters;
    bool handler_attached = false;
};

void param_type_check(const json::value* param, const param_definition* def);
void param_type_check(const json::value* param, const std::vector<param_definition>& defs);

}

#endif // STRTB_EVENT_ITEM_H
