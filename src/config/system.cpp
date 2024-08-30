#include "system.h"
#include "../json/all_value_types.h"
#include "../json/parser.h"
#include "../common/strescape.h"

#include <fstream>

using namespace strtb;
using namespace strtb::config;

std::string system::to_lowercase(const std::string &category_name) {
    std::string result;
    result.reserve(category_name.size());
    // Convert to lowercase and check for invalid characters or invalid names
    for (unsigned char c : category_name)
        result.push_back(std::tolower(c));
    return result;
}

system::category_instance& system::find_category(const std::string &category_name, bool changed) {
    try {
        category_instance &cat = _categories.at(to_lowercase(category_name));
        if (changed)
            cat.changed = true;
        return cat;
    } catch (std::out_of_range &e) {
        throw category_not_found(to_lowercase(category_name));
    }
}

json::value* system::find_next_item(json::value* current, const id_type &go_to) {
    if (current->type() != go_to.type()) {
        // Invalid ID-type for the current value (e.g. key given for a value_array).
        // Should be converted to config::broken_path exception upstream.
        throw std::out_of_range("Broken config path");
    }

    switch (go_to.type()) {
        // Both will throw std::out_of_range if the key or position isn't found.
        // Should be converted to config::broken_path exception upstream.

    case json::VAL_ARRAY:
        return &((json::value_array*) current)->at(go_to.pos());
        break;

    case json::VAL_OBJECT:
        return &((json::value_object*) current)->at(go_to.key());
        break;

    default: // This is here to suppress compiler warnings, and to error out on any unexpected situations that are normally impossible.
        throw std::runtime_error("Reached an unreachable state. This is either a very weird bug, or your hardware is unstable.");
    }
}

json::value* system::follow_path(const std::string &category_name, const path_type &path, bool skip_last) {
    json::value* current = find_category(category_name).root;
    size_t pos = 0;

    // Follow path
    try {
        if (skip_last) {
            // Follow path before last item
            for (auto id = path.begin(); id < path.end() - 1; id++) {
                current = find_next_item(current, *id);
                pos++;
            }
        } else {
            // Follow full path
            for (auto &id : path) {
                current = find_next_item(current, id);
                pos++;
            }
        }
    } catch (std::out_of_range &e) {
        throw broken_path(category_name, path, pos);
    }

    return current;
}

system::system(const std::filesystem::path &config_dir) : log("Config System"), _config_dir(config_dir) {
    log.put(logging::DEBUG, {"Initialized with config dir ", common::string_escape(config_dir)});
}

system::~system() {
    // Delete data from any category that's still loaded
    for (auto cat : _categories)
        delete cat.second.root;
}

void system::load_category(const std::string &category_name) {
    std::lock_guard<std::mutex> guard(_lock);
    try {
        // Check if it's already loaded
        find_category(category_name);
    } catch (category_not_found&) {
        // Make sure it's a valid category name
        for (unsigned char c : category_name) {
            bool invalid = false;
            switch (c) {
            case '<':
            case '>':
            case ':':
            case '"':
            case '/':
            case '\\':
            case '|':
            case '?':
            case '*':
                invalid = true;
                break;
            default:
                if (c < 32)
                    invalid = true;
            }
            if (invalid)
                throw invalid_category_name(category_name, c);
        }
        // Category names are case-insensitive (Windows filesystem limitation)
        std::string cat_lower = to_lowercase(category_name);
        // Prepend "config_" to filename to avoid hitting a Windows reserved name (e.g. CON, PRN, COM#, etc.)
        // These Windows-specific things are done, regardless of the current platform, for portability reasons.
        category_instance instance {
            .path = _config_dir / ("config_" + cat_lower + ".json"),
            .root = nullptr // silences a compiler warning
        };

        // Create or load config file
        try {
            switch (std::filesystem::status(instance.path).type()) {
            case std::filesystem::file_type::regular:
                // Config file exists, load it.
                try {
                    instance.root = json::parser::from_file(instance.path.c_str());
                    log.put(logging::DEBUG, {"Loaded category ", common::string_escape(cat_lower)});
                    break;
                } catch (json::parser::invalid_json &e) {
                    log.put(logging::WARNING, {"Failed to load category ", common::string_escape(cat_lower), ": ", e.what(), ". Backing up config file and creating a new empty one."});
                    std::filesystem::rename(instance.path, std::string(instance.path) + "-invalid.bak");
                }

            case std::filesystem::file_type::not_found:
                // Config file doesn't exist, create it.
                {
                    // Make sure we can actually create the file
                    std::ofstream test_out(instance.path);
                    test_out.exceptions(std::ostream::failbit | std::ostream::badbit);
                    test_out << std::endl;
                    test_out.close();
                }
                instance.root = new json::value_object();
                log.put(logging::DEBUG, {"Created category ", common::string_escape(cat_lower)});
                break;

            default:
                // Something else exists (e.g. directory) in the place of this config file.
                std::string error_msg = "path " + common::string_escape(instance.path) + " exists, but doesn't lead to a file";
                log.put(logging::ERROR, {"Failed to load category ", common::string_escape(cat_lower), ": ", error_msg});
                throw category_filesystem_error(error_msg);
            }
        } catch (std::iostream::failure &e) {
            log.put(logging::ERROR, {"Failed to load category ", common::string_escape(cat_lower), " due to filesystem error: ", e.what()});
            throw category_filesystem_error(e.what());
        }

        // Add this instance to category map
        _categories[cat_lower] = instance;
    }
}

void system::save_category(const std::string &category_name, bool force) {
    std::lock_guard<std::mutex> guard(_lock);
    category_instance &cat = find_category(category_name);
    // Only save if there are changes or if force=true
    if (cat.changed || force)
        cat.root->write_to_file(cat.path.c_str(), 4);
    cat.changed = false;
}

void system::close_category(const std::string &category_name, bool save_if_needed) {
    if (save_if_needed)
        save_category(category_name);

    std::lock_guard<std::mutex> guard(_lock);
    category_instance &cat = find_category(category_name);
    delete cat.root;
    _categories.erase(to_lowercase(category_name));
}

json::value* system::get_category_root(const std::string &category_name) {
    std::lock_guard<std::mutex> guard(_lock);
    return find_category(category_name).root->copy();
}

json::val_type system::get_category_root_type(const std::string &category_name) {
    std::lock_guard<std::mutex> guard(_lock);
    return find_category(category_name).root->type();
}

void system::set_category_root(const std::string &category_name, const json::val_type type) {
    std::lock_guard<std::mutex> guard(_lock);
    json::value_utils::change_default(&find_category(category_name, true).root, type);
}

void system::set_category_root(const std::string &category_name, const json::value_auto &value) {
    std::lock_guard<std::mutex> guard(_lock);
    json::value_utils::change_auto(&find_category(category_name, true).root, value);
}

void system::set_category_root_move(const std::string &category_name, json::value *value) {
    std::lock_guard<std::mutex> guard(_lock);
    category_instance &cat = find_category(category_name, true);
    delete cat.root;
    cat.root = value;
}

void system::clear_category_root(const std::string &category_name) {
    std::lock_guard<std::mutex> guard(_lock);
    category_instance &cat = find_category(category_name, true);
    switch (cat.root->type()) {
    case json::VAL_ARRAY:
        ((json::value_array*) cat.root)->clear();
        break;

    case json::VAL_OBJECT:
        ((json::value_object*) cat.root)->clear();
        break;

    default:
        throw invalid_target(__func__, cat.root->type());
    }
}

bool system::exists(const std::string &category_name, const path_type &path) {
    std::lock_guard<std::mutex> guard(_lock);

    // Try finding the category and following the entire path
    try {
        follow_path(category_name, path);
    } catch (category_not_found &e) {
        // False if category doesn't exist
        return false;
    } catch (broken_path &e) {
        // False if something in the path doesn't exist
        return false;
    }

    return true;
}

json::val_type system::get_type(const std::string &category_name, const path_type &path) {
    // Empty path = root
    if (path.empty())
        return get_category_root_type(category_name);

    std::lock_guard<std::mutex> guard(_lock);
    try {
        // Return type
        return follow_path(category_name, path)->type();
    } catch (broken_path &e) {
        // Undefined type, if it doesn't exist
        return json::VAL_UNDEFINED;
    }
}

json::value* system::get_value(const std::string &category_name, const path_type &path) {
    // Empty path = root
    if (path.empty())
        return get_category_root(category_name);

    std::lock_guard<std::mutex> guard(_lock);
    return follow_path(category_name, path)->copy();
}

void system::set_value(const std::string &category_name, const path_type &path, json::val_type type) {
    // Empty path = root
    if (path.empty()) {
        set_category_root(category_name, type);
        return;
    }
    std::lock_guard<std::mutex> guard(_lock);
    json::value* current = follow_path(category_name, path, true);

    try {
        // Set value to last path item
        if (current->type() != path.back().type()) {
            // Broken path on last item
            throw std::out_of_range("");    // Will be caught and converted to broken_path exception
        }
        switch (path.back().type()) {
        case json::VAL_ARRAY:
            ((json::value_array*) current)->set(path.back().pos(), type);
            break;

        case json::VAL_OBJECT:
            ((json::value_object*) current)->set(path.back().key(), type);
            break;

        default: // This is here to suppress compiler warnings, and to error out on any unexpected situations that are normally impossible.
            throw std::runtime_error("Reached an unreachable state. This is either a very weird bug, or your hardware is unstable.");
        }
    } catch (std::out_of_range &e) {
        throw broken_path(category_name, path);
    }

    find_category(category_name).changed = true;
}

void system::set_value(const std::string &category_name, const path_type &path, const json::value_auto &value) {
    // Empty path = root
    if (path.empty()) {
        set_category_root(category_name, value);
        return;
    }
    std::lock_guard<std::mutex> guard(_lock);
    json::value* current = follow_path(category_name, path, true);

    try {
        // Set value to last path item
        if (current->type() != path.back().type()) {
            // Broken path on last item
            throw std::out_of_range("");    // Will be caught and converted to broken_path exception
        }
        switch (path.back().type()) {
        case json::VAL_ARRAY:
            ((json::value_array*) current)->set(path.back().pos(), value);
            break;

        case json::VAL_OBJECT:
            ((json::value_object*) current)->set(path.back().key(), value);
            break;

        default: // This is here to suppress compiler warnings, and to error out on any unexpected situations that are normally impossible.
            throw std::runtime_error("Reached an unreachable state. This is either a very weird bug, or your hardware is unstable.");
        }
    } catch (std::out_of_range &e) {
        throw broken_path(category_name, path);
    }

    find_category(category_name).changed = true;
}

void system::set_value_move(const std::string &category_name, const path_type &path, json::value *value) {
    // Empty path = root
    if (path.empty()) {
        set_category_root_move(category_name, value);
        return;
    }
    std::lock_guard<std::mutex> guard(_lock);
    json::value* current = follow_path(category_name, path, true);

    try {
        // Set value to last path item
        if (current->type() != path.back().type()) {
            // Broken path on last item
            throw std::out_of_range("");    // Will be caught and converted to broken_path exception
        }
        switch (path.back().type()) {
        case json::VAL_ARRAY:
            ((json::value_array*) current)->set_move(path.back().pos(), value);
            break;

        case json::VAL_OBJECT:
            ((json::value_object*) current)->set_move(path.back().key(), value);
            break;

        default: // This is here to suppress compiler warnings, and to error out on any unexpected situations that are normally impossible.
            throw std::runtime_error("Reached an unreachable state. This is either a very weird bug, or your hardware is unstable.");
        }
    } catch (std::out_of_range &e) {
        throw broken_path(category_name, path);
    }

    find_category(category_name).changed = true;
}

void system::erase_value(const std::string &category_name, const path_type &path) {
    // Empty path = no target specified to erase
    if (path.empty())
        throw invalid_target(__func__);
    std::lock_guard<std::mutex> guard(_lock);
    json::value* current = follow_path(category_name, path, true);

    try {
        // Erase last path item
        if (current->type() != path.back().type()) {
            // Broken path on last item
            throw std::out_of_range("");    // Will be caught and converted to broken_path exception
        }
        switch (path.back().type()) {
        case json::VAL_ARRAY:
            ((json::value_array*) current)->erase(path.back().pos());
            break;

        case json::VAL_OBJECT:
            ((json::value_object*) current)->erase(path.back().key());
            break;

        default: // This is here to suppress compiler warnings, and to error out on any unexpected situations that are normally impossible.
            throw std::runtime_error("Reached an unreachable state. This is either a very weird bug, or your hardware is unstable.");
        }
    } catch (std::out_of_range &e) {
        throw broken_path(category_name, path);
    }

    find_category(category_name).changed = true;
}

void system::write_to_stream(const std::string &category_name, const path_type &path, std::ostream &stream, int pretty_print, const char* newline) {
    std::lock_guard<std::mutex> guard(_lock);
    follow_path(category_name, path)->write_to_stream(stream, pretty_print, newline);
}

std::string system::write_to_string(const std::string &category_name, const path_type &path, int pretty_print, const char* newline) {
    std::lock_guard<std::mutex> guard(_lock);
    return follow_path(category_name, path)->write_to_string(pretty_print, newline);
}

size_t system::size(const std::string &category_name, const path_type &path) {
    std::lock_guard<std::mutex> guard(_lock);
    json::value* current = follow_path(category_name, path);

    switch (current->type()) {
    case json::VAL_ARRAY:
        return ((json::value_array*) current)->size();
    case json::VAL_OBJECT:
        return ((json::value_object*) current)->size();
    default:
        throw invalid_target(__func__, current->type());
    }
}

void system::clear(const std::string &category_name, const path_type &path) {
    std::lock_guard<std::mutex> guard(_lock);
    json::value* current = follow_path(category_name, path);

    switch (current->type()) {
    case json::VAL_ARRAY:
        ((json::value_array*) current)->clear();
        break;
    case json::VAL_OBJECT:
        ((json::value_object*) current)->clear();
        break;
    default:
        throw invalid_target(__func__, current->type());
    }

    find_category(category_name).changed = true;
}

json::value* system::array_get_back(const std::string &category_name, const path_type &path) {
    std::lock_guard<std::mutex> guard(_lock);
    json::value* current = follow_path(category_name, path);

    if (current->type() == json::VAL_ARRAY)
        return ((json::value_array*) current)->back();
    else
        throw invalid_target(__func__, current->type());
}

void system::array_push_back(const std::string &category_name, const path_type &path, json::val_type type) {
    std::lock_guard<std::mutex> guard(_lock);
    json::value* current = follow_path(category_name, path);

    if (current->type() == json::VAL_ARRAY)
        ((json::value_array*) current)->push_back(type);
    else
        throw invalid_target(__func__, current->type());

    find_category(category_name).changed = true;
}

void system::array_push_back(const std::string &category_name, const path_type &path, const json::value_auto &value) {
    std::lock_guard<std::mutex> guard(_lock);
    json::value* current = follow_path(category_name, path);

    if (current->type() == json::VAL_ARRAY)
        ((json::value_array*) current)->push_back(value);
    else
        throw invalid_target(__func__, current->type());

    find_category(category_name).changed = true;
}

void system::array_push_back_move(const std::string &category_name, const path_type &path, json::value* value) {
    std::lock_guard<std::mutex> guard(_lock);
    json::value* current = follow_path(category_name, path);

    if (current->type() == json::VAL_ARRAY)
        ((json::value_array*) current)->push_back_move(value);
    else
        throw invalid_target(__func__, current->type());

    find_category(category_name).changed = true;
}

void system::array_pop_back(const std::string &category_name, const path_type &path) {
    std::lock_guard<std::mutex> guard(_lock);
    json::value* current = follow_path(category_name, path);

    if (current->type() == json::VAL_ARRAY)
        ((json::value_array*) current)->pop_back();
    else
        throw invalid_target(__func__, current->type());

    find_category(category_name).changed = true;
}

json::value* system::array_pop_and_get_back(const std::string &category_name, const path_type &path) {
    std::lock_guard<std::mutex> guard(_lock);
    json::value* current = follow_path(category_name, path);

    if (current->type() == json::VAL_ARRAY) {
        json::value* back = ((json::value_array*) current)->back();
        ((json::value_array*) current)->pop_back();
        find_category(category_name).changed = true;
        return back;
    } else
        throw invalid_target(__func__, current->type());
}

void system::array_insert(const std::string &category_name, const path_type &path, size_t pos, json::val_type type) {
    std::lock_guard<std::mutex> guard(_lock);
    json::value* current = follow_path(category_name, path);

    if (current->type() == json::VAL_ARRAY)
        ((json::value_array*) current)->insert(pos, type);
    else
        throw invalid_target(__func__, current->type());

    find_category(category_name).changed = true;
}

void system::array_insert(const std::string &category_name, const path_type &path, size_t pos, const json::value_auto &value) {
    std::lock_guard<std::mutex> guard(_lock);
    json::value* current = follow_path(category_name, path);

    if (current->type() == json::VAL_ARRAY)
        ((json::value_array*) current)->insert(pos, value);
    else
        throw invalid_target(__func__, current->type());

    find_category(category_name).changed = true;
}

void system::array_insert_move(const std::string &category_name, const path_type &path, size_t pos, json::value *value) {
    std::lock_guard<std::mutex> guard(_lock);
    json::value* current = follow_path(category_name, path);

    if (current->type() == json::VAL_ARRAY)
        ((json::value_array*) current)->insert_move(pos, value);
    else
        throw invalid_target(__func__, current->type());

    find_category(category_name).changed = true;
}
