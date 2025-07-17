#include "item.h"

using namespace strtb::event;

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
    for (auto i : object_definition)
        delete i;

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

example_definition::example_definition(const example_definition& other) {
    params = other.params;
    returns = other.returns;
}

example_definition::example_definition(example_definition&& other) {
    params = std::move(other.params);
    returns = std::move(other.returns);
}

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
