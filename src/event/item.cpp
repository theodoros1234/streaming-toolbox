#include "item.h"
#include "../json/cast.h"

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
parsing_error::parsing_error(const std::string& what) : event_exception(what) {}

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

param_definition::param_definition(const json::value_object* from) {
    if (!from)
        throw std::invalid_argument("argument can't be a null pointer");

    try {
        // Name
        try {
            name = json::cast_string(&from->at("name"))->value();
        } catch (std::out_of_range&) {
        } catch (json::wrong_type&) {
            throw parsing_error("\"name\" is not a string");
        }

        // Type
        try {
            std::string type_str = json::cast_string(&from->at("type"))->value();
            type = json::type_from_string(type_str);
            if (type == json::VAL_UNDEFINED || type == json::VAL_NULL)
                throw parsing_error("\"type\" does not name an accepted type");
        } catch (std::out_of_range&) {
        } catch (json::wrong_type&) {
            throw parsing_error("\"type\" is not a string");
        }

        // Required
        try {
            required = json::cast_bool(&from->at("required"))->value();
        } catch (std::out_of_range&) {
        } catch (json::wrong_type&) {
            throw parsing_error("\"required\" is not a bool");
        }

        // Array definition
        if (type == json::VAL_ARRAY) {
            try {
                array_definition = new param_definition(json::cast_object(&from->at("array_definition")));
            } catch (std::out_of_range&) {
            } catch (json::wrong_type&) {
                throw parsing_error("\"array_definition\" is not an object");
            }
        }

        // Object definition
        if (type == json::VAL_OBJECT) {
            try {
                const json::value_array* v = json::cast_array(&from->at("object_definition"));
                for (auto i : *v) {
                    param_definition* ptr = nullptr;

                    try {
                        ptr = new param_definition(json::cast_object(i));
                    } catch (json::wrong_type&) {
                        throw parsing_error("\"object_definition\" contains a child value that isn't an object");
                    }

                    try {
                        object_definition.push_back(ptr);
                    } catch (...) {
                        delete ptr;
                        throw;
                    }
                }
            } catch (std::out_of_range&) {
            } catch (json::wrong_type&) {
                throw parsing_error("\"object_definition\" is not an array");
            }
        }

    } catch (...) {
        // Clean up any created objects if there's an exception to avoid leaking memory
        if (array_definition)
            delete array_definition;
        for (auto i : object_definition)
            delete i;
        throw;
    }
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

example_definition::example_definition(const json::value_object* params, const json::value_object* returns) {
    if (params == nullptr || returns == nullptr)
        throw std::invalid_argument("parmas and returns can't be null");
    this->params = *params;
    this->returns = *returns;
}

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

item_info::item_info(const json::value_object* from) {
    try {
        std::string type_str = json::cast_string(&from->at("type"))->value();
        if (type_str == "event_src")
            type = ITEM_EVENT_SRC;
        else if (type_str == "action_sink")
            type = ITEM_ACTION_SINK;
        else if (type_str == "category")
            type = ITEM_CATEGORY;
        else
            throw parsing_error("\"type\" does not name a valid type");
    } catch (std::out_of_range&) {
        throw parsing_error("\"type\" is missing");
    } catch (json::wrong_type&) {
        throw parsing_error("\"type\" must be a string");
    }

    try {
        display_name = json::cast_string(&from->at("display_name"))->value();
    } catch (std::out_of_range&) {
        throw parsing_error("\"display_name\" is missing");
    } catch (json::wrong_type&) {
        throw parsing_error("\"display_name\" must be a string");
    }

    try {
        description = json::cast_string(&from->at("description"))->value();
    } catch (std::out_of_range&) {
        throw parsing_error("\"description\" is missing");
    } catch (json::wrong_type&) {
        throw parsing_error("\"description\" must be a string");
    }

    if (type == ITEM_EVENT_SRC || type == ITEM_ACTION_SINK) {
        // For events/actions: get parameter/return definition and examples

        // params
        try {
            const json::value_array* param_list = json::cast_array(&from->at("params"));
            for (json::value* i : *param_list) {    // TODO: make i const after adding const JSON cast functions
                // Parse all parameters
                try {
                    params.emplace_back(json::cast_object(i));
                } catch (parsing_error& e) {
                    throw parsing_error(std::string("error parsing a value inside \"params\": ") + e.what());
                } catch (json::wrong_type&) {
                    throw parsing_error("\"params\" must only contain objects");
                }
            }
        } catch (std::out_of_range&) {  // ignored, maybe it doesn't take params
        } catch (json::wrong_type&) {
            throw parsing_error("\"params\" must be an array");
        }

        // returns
        try {
            const json::value_array* return_list = json::cast_array(&from->at("returns"));
            for (json::value* i : *return_list) {    // TODO: make i const after adding const JSON cast functions
                // Parse all returneters
                try {
                    returns.emplace_back(json::cast_object(i));
                } catch (parsing_error& e) {
                    throw parsing_error(std::string("error parsing a value inside \"returns\": ") + e.what());
                } catch (json::wrong_type&) {
                    throw parsing_error("\"returns\" must only contain objects");
                }
            }
        } catch (std::out_of_range&) {  // ignored, maybe it doesn't return anything
        } catch (json::wrong_type&) {
            throw parsing_error("\"returns\" must be an array");
        }

        // examples
        try {
            const json::value_array* example_list = json::cast_array(&from->at("examples"));
            for (json::value* i : *example_list) {  // TODO: make i const after adding const JSON cast functions
                const json::value_object *i_params = nullptr, *i_returns = nullptr;

                try {
                    i_params = json::cast_object(&json::cast_object(i)->at("params"));
                    i_returns = json::cast_object(&json::cast_object(i)->at("returns"));
                } catch (json::wrong_type&) {
                    throw parsing_error("invalid example: must be an object that contains values \"params\" and \"returns\" as objects");
                } catch (std::out_of_range&) {
                    throw parsing_error("invalid example: must contain \"params\" and \"returns\"");
                }

                examples.emplace_back(i_params, i_returns);
            }
        } catch (std::out_of_range&) {  // ignored, maybe it doesn't give examples
        } catch (json::wrong_type&) {
            throw parsing_error("\"examples\" must be an array");
        }
    }

}
