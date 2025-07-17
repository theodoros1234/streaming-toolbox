#ifndef STRTB_EVENT_ITEM_H
#define STRTB_EVENT_ITEM_H

#include <string>
#include <vector>
#include "../json/value_object.h"
#include <stdint.h>

namespace strtb::event {

struct param_definition {
    std::string name;
    json::val_type type = json::VAL_UNDEFINED;
    bool required = false;
    param_definition* array_definition = nullptr;
    std::vector<param_definition*> object_definition;

    param_definition() = default;
    param_definition(const param_definition& from);
    param_definition(param_definition&& from);
    ~param_definition();
    param_definition& operator=(const param_definition& from);
    param_definition& operator=(param_definition&& from);
};

struct example_definition {
    json::value_object params, returns;

    example_definition() = default;
    example_definition(const example_definition& other);
    example_definition(example_definition&& other);
    example_definition& operator=(const example_definition& other);
    example_definition& operator=(example_definition&& other);
};

enum item_type {ITEM_UNDEFINED, ITEM_CATEGORY, ITEM_EVENT_SRC, ITEM_ACTION_SINK};

typedef std::vector<std::string> item_path;

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
};

struct item_listing : public item_info {
    uint64_t resource_id = 0;
    std::string name;
};

}

#endif // STRTB_EVENT_ITEM_H
