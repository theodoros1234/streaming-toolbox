#include "item.h"
#include "../json/cast.h"
#include "../common/strescape.h"

using namespace strtb::event;

event_exception::event_exception(const std::string& what) : _what(what) {}

const char* event_exception::what() const noexcept {
    return _what.c_str();
}

internal_error::internal_error(const std::string& what) : event_exception(what) {}

internal_error::internal_error(const std::string& what, logging::source& log_to) : event_exception(what) {
    log_to.put(logging::ERROR, {"Internal error: ", what});
}

not_found::not_found(const std::string& what) : event_exception(what) {}
wrong_type::wrong_type(const std::string& what) : event_exception(what) {}
out_of_scope::out_of_scope(const std::string& what) : event_exception(what) {}
already_exists::already_exists(const std::string& what) : event_exception(what) {}
parsing_error::parsing_error(const std::string& what) : event_exception(what) {}
invalid_path::invalid_path(const std::string& what, ssize_t pos) : event_exception(what), pos(pos) {}
bad_definition::bad_definition(const std::string& what) : event_exception(what) {}
bad_state::bad_state(const std::string& what) : event_exception(what) {}

param_definition::param_definition(const param_definition& from) :
    name(from.name),
    description(from.description),
    type(from.type),
    required(from.required) {

    if (from.array_definition)
        array_definition = new param_definition(*from.array_definition);

    try {
        if (!from.object_definition.empty()) {
            object_definition.assign(from.object_definition.size(), nullptr);
            for (size_t i=0; i<object_definition.size(); i++)
                object_definition[i] = new param_definition(*from.object_definition[i]);
        }
    } catch (...) {
        if (array_definition)
            delete array_definition;
        for (auto i : object_definition)
            if (i)
                delete i;
        throw;
    }
}

param_definition::param_definition(param_definition&& from) :
    name(std::move(from.name)),
    description(std::move(from.description)),
    type(from.type),
    required(from.required),
    array_definition(from.array_definition),
    object_definition(std::move(from.object_definition))
{
    from.type = json::VAL_UNDEFINED;
    from.array_definition = nullptr;
    from.object_definition.clear();
}

param_definition::param_definition(json::val_type type) : type(type) {}

param_definition::param_definition(const std::string& name,
                                   const std::string& description,
                                   json::val_type type,
                                   bool required) :
    name(name), description(description), type(type), required(required) {}

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

        // Description
        try {
            description = json::cast_string(&from->at("description"))->value();
        } catch (std::out_of_range&) {
        } catch (json::wrong_type&) {
            throw parsing_error("\"description\" is not a string");
        }

        // Type
        try {
            std::string type_str = json::cast_string(&from->at("type"))->value();
            type = json::type_from_string(type_str);
            // DON'T remove the following check. If the type is unspecified, it can just be ommited.
            if (type == json::VAL_UNDEFINED || type == json::VAL_NULL)
                throw parsing_error(common::string_escape(type_str) + " is not an accepted type");
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

    try {
        // Copy attributes from the other object
        name = from.name;
        description = from.description;
        type = from.type;
        required = from.required;

        if (from.array_definition)
            array_definition = new param_definition(*from.array_definition);

        if (!from.object_definition.empty()) {
            object_definition.assign(from.object_definition.size(), nullptr);
            for (size_t i=0; i<object_definition.size(); i++)
                object_definition[i] = new param_definition(*from.object_definition[i]);
        }
    } catch (...) {
        if (array_definition)
            delete array_definition;
        array_definition = nullptr;
        for (auto i : object_definition)
            if (i)
                delete i;
        object_definition.clear();
        name.clear();
        description.clear();
        type = json::VAL_UNDEFINED;
        required = false;
        throw;
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
    description = std::move(from.description);
    type = from.type;
    required = from.required;

    array_definition = from.array_definition;
    from.array_definition = nullptr;

    object_definition = std::move(from.object_definition);
    from.object_definition.clear();

    return *this;
}

void param_definition::set(const std::string& name,
                           const std::string& description,
                           json::val_type type,
                           bool required) {
    this->name = name;
    this->description = description;
    this->type = type;
    this->required = required;
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

void param_definition::object_add_definition(const std::string& name, const std::string &description, json::val_type type, bool required) {
    if (this->type != json::VAL_OBJECT)
        throw wrong_type("not an object");

    param_definition* new_def = new param_definition(name, description, type, required);
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

example_definition::example_definition(const json::value* params, const json::value *returns) {
    if (params == nullptr || returns == nullptr)
        throw std::invalid_argument("params and returns can't be null");
    this->params = params;
    this->returns = returns;
}

example_definition& example_definition::operator=(const example_definition& other) {
    try {
        params = other.params;
        returns = other.returns;
    } catch (...) {
        params.clear();
        returns.clear();
        throw;
    }

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
            throw parsing_error(common::string_escape(type_str) + " is not a valid item type");
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

    // For events/actions: get parameter/return definition and examples
    if (type == ITEM_EVENT_SRC) {
        // single param
        try {
            params.emplace_back(json::cast_object(&from->at("param")));
        } catch (std::out_of_range&) {  // ignored, maybe it doesn't take a parameter
        } catch (json::wrong_type&) {
            throw parsing_error("\"param\" must be an object, or must be ommited"
                                "if the event source doesn't take a parameter");
        } catch (parsing_error& e) {
            throw parsing_error(std::string("error parsing parameter definition: ") + e.what());
        }
    } else if (type == ITEM_ACTION_SINK) {
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
    }

    if (type == ITEM_EVENT_SRC || type == ITEM_ACTION_SINK) {
        // returns
        try {
            returns = json::cast_object(&from->at("returns"));
        } catch (std::out_of_range&) {  // ignored, maybe it doesn't return anything
        } catch (json::wrong_type&) {
            throw parsing_error("\"returns\" must be an array");
        }

        // examples
        try {
            const json::value_array* example_list = json::cast_array(&from->at("examples"));
            for (json::value* i : *example_list) {  // TODO: make i const after adding const JSON cast functions
                const json::value *i_params = nullptr, *i_returns = nullptr;

                try {
                    i_params = &json::cast_object(i)->at("params");
                    i_returns = &json::cast_object(i)->at("returns");
                } catch (json::wrong_type&) {
                    throw parsing_error("invalid example: must be an object");
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

item_path::item_path(const std::string& path, size_t max_segment_length, size_t max_depth) {
    std::string::size_type from = 0, to = 1, segment_length = 0;
    if (path.size() < 1 || path.at(0) != '/')
        throw parsing_error("path must start with /");

    while (size() <= max_depth) {
        if (to >= path.size())
            return;

        from = to;
        to = path.find('/', from);
        if (to == path.npos) {  // end of string
            to = path.size();
            segment_length = to - from;
        } else {                    // found slash, skip it for next iteration
            segment_length = to - from;
            to++;
        }

        if (segment_length == 0)    // skip empty segments
            continue;

        if (segment_length > max_segment_length)
            throw parsing_error("segment exceeds maximum segment length");

        std::string segment = path.substr(from, segment_length);

        if (!validate_segment(segment))
            throw parsing_error("path contains invalid characters");

        if (size() >= max_depth)
            throw parsing_error("path exceeds maximum depth");

        if (validate_segment(segment))
            push_back(std::move(segment));
        else
            throw parsing_error("path segment " + common::string_escape(segment) + " is invalid");
    }

    throw parsing_error("path exceeds maximum depth");
}

item_path::item_path(const char* path, size_t max_segment_length, size_t max_depth)
    : item_path(std::string(path), max_segment_length, max_depth) {}

std::string item_path::to_string() const {
    if (empty())   // root path
        return "/";

    std::string str;
    ssize_t path_validate = validate();
    if (path_validate != -1)
        throw invalid_path("path segment " + common::string_escape(at(path_validate)) + " is invalid", path_validate);

    size_t length = 0;
    for (const std::string& segment : *this)
        length += segment.length() + 1;
    str.reserve(length);

    for (const std::string& segment : *this) {
        str += "/";
        str += segment;
    }

    return str;
}

ssize_t item_path::validate() const {
    for (item_path::const_iterator segment = begin(); segment < end(); segment++)
        if (!validate_segment(*segment))
            return segment - begin();   // Returning position of problematic segment
    return -1;  // Returning -1 means all segments are valid
}

bool item_path::validate_segment(size_t pos) const {
    return validate_segment(at(pos));
}

bool item_path::validate_segment(const std::string& segment) {
    if (segment.empty())
        return false;
    for (char c : segment)
        if (!(('a' <= c && c <= 'z') ||
              ('A' <= c && c <= 'Z') ||
              ('0' <= c && c <= '9') ||
              (c == '_') || (c == '-')))
            return false;
    return true;
}

void strtb::event::param_type_check(const json::value* param, const param_definition* def) {
    if (!((param->type() == def->type) ||                           // type is correct
          (param->type() == json::VAL_NULL && !def->required) ||    // type is null when it's not required
          (def->type == json::VAL_UNDEFINED)))                      // required type not defined
        throw wrong_type("type does not match the definition");

    if (param->type() == json::VAL_ARRAY &&
        def->array_definition != nullptr &&
        def->array_definition->type != json::VAL_UNDEFINED) {
        const json::value_array* param_arr = json::cast_array(param);
        for (size_t i = 0; i < param_arr->size(); i++) {
            try {
                param_type_check(&param_arr->at(i), def->array_definition);
            } catch (wrong_type& e) {
                throw wrong_type("at " + std::to_string(i) + ": " + e.what());
            }
        }
    }

    if (param->type() == json::VAL_OBJECT) {
        // NOTE: extra keys in param won't cause an error
        const json::value_object* param_obj = json::cast_object(param);
        for (const param_definition* subdef : def->object_definition) {
            if (!subdef->required && subdef->type == json::VAL_UNDEFINED)
                continue;

            try {
                param_type_check(&param_obj->at(subdef->name), subdef);
            } catch (std::out_of_range&) {
                if (subdef->required)
                    throw wrong_type("missing required key " + common::string_escape(subdef->name));
            } catch (wrong_type& e) {
                throw wrong_type("at " + common::string_escape(subdef->name) + ": " + e.what());
            }
        }
    }
}

void strtb::event::param_type_check(const json::value* param, const std::vector<param_definition>& defs) {
    if (param == nullptr)
        throw wrong_type("param cannot be nullptr");
    if (param->type() != json::VAL_OBJECT)
        throw wrong_type("must be an object");

    // NOTE: extra keys in param won't cause an error
    const json::value_object* param_obj = json::cast_object(param);
    for (const param_definition& subdef : defs) {
        if (!subdef.required && subdef.type == json::VAL_UNDEFINED)
            continue;

        try {
            param_type_check(&param_obj->at(subdef.name), &subdef);
        } catch (std::out_of_range&) {
            if (subdef.required)
                throw wrong_type("missing required key " + subdef.name);
        } catch (wrong_type& e) {
            throw wrong_type("at " + common::string_escape(subdef.name) + ": " + e.what());
        }
    }
}
