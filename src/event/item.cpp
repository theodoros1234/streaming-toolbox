#include "item.h"

using namespace strtb::event;

event_exception::event_exception(const std::string& what) : _what(what) {}

const char* event_exception::what() const noexcept {
    return _what.c_str();
}

internal_error::internal_error(const std::string& what) : event_exception(what) {}
not_found::not_found(const std::string& what) : event_exception(what) {}
wrong_type::wrong_type(const std::string& what) : event_exception(what) {}
out_of_scope::out_of_scope(const std::string& what) : event_exception(what) {}
already_exists::already_exists(const std::string& what) : event_exception(what) {}

param_definition::param_definition(const param_definition& from) {
    name = from.name;
    type = from.type;
    required = from.required;

    if (from.array_definition)
        array_definition = new param_definition(*from.array_definition);

    if (!from.object_definition.empty()) {
        object_definition.resize(from.object_definition.size());
        for (size_t i=0; i<from.object_definition.size(); i++)
            object_definition.at(i) = new param_definition(*from.object_definition.at(i));
    }
}

param_definition::param_definition(param_definition&& from) :
    name(std::move(from.name)),
    type(from.type),
    required(from.required),
    array_definition(from.array_definition),
    object_definition(from.object_definition)
{
    from.type = json::VAL_UNDEFINED;
    from.array_definition = nullptr;
    from.object_definition.clear();
}

param_definition::param_definition(json::val_type type) : type(type) {}

param_definition::param_definition(const std::string& name, json::val_type type, bool required) :
    name(name), type(type), required(required) {}

param_definition::~param_definition() {
    if (array_definition)
        delete array_definition;
    for (auto i : object_definition)
        delete i;
}

param_definition& param_definition::operator=(const param_definition& from) {
    // Delete anything we have
    if (array_definition)
        delete array_definition;
    array_definition = nullptr;
    for (auto i : object_definition)
        delete i;
    object_definition.clear();

    // Copy attributes from the other object
    name = from.name;
    type = from.type;
    required = from.required;

    if (from.array_definition)
        array_definition = new param_definition(*from.array_definition);

    if (!from.object_definition.empty()) {
        object_definition.resize(from.object_definition.size());
        for (size_t i=0; i<from.object_definition.size(); i++)
            object_definition.at(i) = new param_definition(*from.object_definition.at(i));
    }

    return *this;
}

param_definition& param_definition::operator=(param_definition&& from) {
    // Delete anything we have
    if (array_definition)
        delete array_definition;
    array_definition = nullptr;
    for (auto i : object_definition)
        delete i;
    object_definition.clear();

    // Copy attributes from the other object
    name = std::move(from.name);
    type = from.type;
    required = from.required;

    if (from.array_definition) {
        array_definition = from.array_definition;
        from.array_definition = nullptr;
    }

    object_definition = std::move(from.object_definition);
    from.object_definition.clear();

    return *this;
}

void param_definition::array_define(json::val_type type) {
    if (this->type != json::VAL_ARRAY)
        throw wrong_type("not an array");

    if (array_definition) {
        delete array_definition;
        array_definition = nullptr;
    }

    array_definition = new param_definition(type);
}

void param_definition::array_clear_definition() {
    if (array_definition) {
        delete array_definition;
        array_definition = nullptr;
    }
}

void param_definition::object_add_definition(const std::string& name, json::val_type type, bool required) {
    if (this->type != json::VAL_OBJECT)
        throw wrong_type("not an object");

    param_definition* new_def = new param_definition(name, type, required);
    try {
        object_definition.push_back(new_def);
    } catch (...) {
        delete new_def;
        throw;
    }
}

void param_definition::object_clear_definitions() {
    for (auto i : object_definition)
        delete i;
    object_definition.clear();
}

example_definition::example_definition(const example_definition& other) {
    params = other.params;
    returns = other.returns;
}

example_definition::example_definition(example_definition&& other) {
    params = std::move(other.params);
    returns = std::move(other.returns);
}

example_definition::example_definition(const json::value_object* params, const json::value_object* returns) :
    example_definition(*params, *returns) {}

example_definition::example_definition(const json::value_object& params, const json::value_object& returns) :
    params(params), returns(returns) {}

example_definition::example_definition(json::value_object&& params, json::value_object&& returns) :
    params(params), returns(returns) {}

example_definition& example_definition::operator=(const example_definition& other) {
    params = other.params;
    returns = other.returns;
    return *this;
}

example_definition& example_definition::operator=(example_definition&& other) {
    params = std::move(other.params);
    returns = std::move(other.returns);
    return *this;
}

item_info::item_info(item_type type, const std::string& display_name, const std::string& description) :
    type(type), display_name(display_name), description(description) {}
