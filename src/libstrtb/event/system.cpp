#include <stdexcept>
#include <cstdint>
#include "system.h"
#include "../logging/logging.h"
#include "../common/strescape.h"
#include "../json/cast.h"

using namespace strtb;
using namespace strtb::event;

event::system* strtb::event::system_ptr = nullptr;
static logging::source log("Event System");

system::res_cnt::res_cnt(res_item* ptr) : ptr(ptr) {}

void system::res_cnt::make_item(item_type type,
                                const std::string& display_name,
                                const std::string& description,
                                uint64_t provider_id) {
    if (ptr != nullptr)
        throw internal_error("resource container already holding an item", log);
    if (type != ITEM_CATEGORY)
        throw internal_error("wrong constructor called for requested item type", log);

    ptr = new res_item_category();
    try {
        ptr->type = type;
        ptr->display_name = display_name;
        ptr->description = description;
        ptr->provider_id = provider_id;
    } catch (...) {
        delete ptr;
        ptr = nullptr;
        throw;
    }
}

void system::res_cnt::make_item(item_type type,
                                const std::string& display_name,
                                const std::string& description,
                                uint64_t provider_id,
                                const std::vector<param_definition>& params,
                                const std::vector<param_definition>& returns,
                                const std::vector<example_definition>& examples) {
    if (ptr != nullptr)
        throw internal_error("resource container already holding an item", log);

    try {
        switch (type) {
        case ITEM_EVENT_SRC: {
            if (params.size() > 1) {
                throw bad_definition("event sources can take at most one parameter");
            } else if (params.size()) {
                switch (params.back().type) {
                case json::VAL_BOOL:
                case json::VAL_INT:
                case json::VAL_STRING:
                    break;
                default:
                    throw wrong_type("event sources can only take a boolean, an integer or a string as a parameter");
                }
            }

            res_item_event_src* ptr_e = new res_item_event_src();
            ptr = ptr_e;
            if (params.size())
                ptr_e->param = params.back();
            ptr_e->returns = returns;
            ptr_e->examples = examples;
        }
        break;

        case ITEM_ACTION_SINK: {
            res_item_action_sink* ptr_a = new res_item_action_sink();
            ptr = ptr_a;
            ptr_a->params = params;
            ptr_a->returns = returns;
            ptr_a->examples = examples;
        }
        break;

        case ITEM_CATEGORY:
            ptr = new res_item_category();
            break;

        default:
            throw wrong_type("invalid item type");
        }

        ptr->type = type;
        ptr->display_name = display_name;
        ptr->description = description;
        ptr->provider_id = provider_id;
    } catch (...) {
        if (ptr)
            delete ptr;
        ptr = nullptr;
        throw;
    }
}

system::res_cnt::res_cnt(res_cnt&& from) {
    ptr = from.ptr;
    from.ptr = nullptr;
}

system::res_cnt::~res_cnt() {
    if (ptr)
        delete ptr;
}

item_type system::res_cnt::type() const {
    if (ptr)
        return ptr->type;
    else
        return ITEM_UNDEFINED;
}

system::res_item_category* system::res_cnt::as_category() const {
    if (!ptr)
        throw not_found("holding a null pointer");
    if (ptr->type != ITEM_CATEGORY)
        throw wrong_type("resource is not a category");
    return (res_item_category*) ptr;
}

system::res_item_event_src* system::res_cnt::as_event_src() const {
    if (!ptr)
        throw not_found("holding a null pointer");
    if (ptr->type != ITEM_EVENT_SRC)
        throw wrong_type("resource is not an event source");
    return (res_item_event_src*) ptr;
}

system::res_item_action_sink* system::res_cnt::as_action_sink() const {
    if (!ptr)
        throw not_found("holding a null pointer");
    if (ptr->type != ITEM_ACTION_SINK)
        throw wrong_type("resource is not an action sink");
    return (res_item_action_sink*) ptr;
}

system::system() {
    if (system_ptr) {
        log.put(logging::CRITICAL, {"Refusing to initialize the event system. Another event system object already exists, or something has tampered with the event::system_ptr pointer, which has a value of ", system_ptr, "."});
        throw std::runtime_error("event system already initialized, or event::system_ptr has been tampered with");
    }
    system_ptr = this;

    // Add root category
    _items[STRTB_EVENT_ROOT].make_item(ITEM_CATEGORY,
                              "Event System Root",
                              "This is the top-level category in the event system, "
                              "which contains sub-categories for all event and action providers.",
                              0);
}

system::~system() {
    system_ptr = nullptr;   // Prevents use-after-free
    // TODO: delete everything in here if necessary (may get deleted by res_holder automatically)
    // maybe just warn about undeleted stuff
}

uint64_t system::_resid_new() {
    if (_resid_counter == UINT64_MAX) {
        log.put(logging::CRITICAL, {"Internal error: Out of available resource IDs. It is likely that "
                                    "this is a bug or that your system is unstable, as it would normally "
                                    "take hundreds of years at minimum for this to happen."});
        throw internal_error("out of available resource ids");
    }
    return _resid_counter++;
}

uint64_t system::_follow_path(uint64_t start, const item_path& path) {
    uint64_t current_pos = start;
    ssize_t path_validate = item_path_validate(path);
    if (path_validate != -1)
        throw invalid_path("path segment " + common::string_escape(path.at(path_validate)) + " is invalid", path_validate);

    for (const std::string& next_piece : path) {
        try {
            res_item_category* cat = _items.at(current_pos).as_category();
            try {
                current_pos = cat->list.at(next_piece);
            } catch (std::out_of_range&) {
                throw not_found(common::string_escape(next_piece) + " was not found");
            }
        } catch (std::out_of_range&) {
            if (current_pos == start)
                throw not_found("resource id " + std::to_string(current_pos) + " not found");
            else // if a category holds an invalid ID, it's very likely our bug, thus throwing internal_error
                throw internal_error("resource id " + std::to_string(current_pos) + " not found", log);
        } catch (wrong_type&) {
            throw wrong_type(common::string_escape(next_piece) + " is not a category");
        }
    }

    return current_pos;
}

system::res_item_category* system::_get_category(uint64_t target_location) {
    try {
        return _items.at(target_location).as_category();
    } catch (std::out_of_range&) {
        throw not_found("target location not found");
    }
}

system::res_item_category* system::_get_category(uint64_t start, const item_path& target_location) {
    try {
        return _items.at(_follow_path(start, target_location)).as_category();
    } catch (std::out_of_range& e) {
        throw internal_error("category entry has an invalid resource id", log);
    }
}

std::pair<uint64_t, system::res_cnt &> system::_provider_item_add(uint64_t provider_id,
                                    res_item_category* location,
                                    const std::string& name,
                                    const item_info& item) {
    if (!item_path_validate_segment(name))
        throw invalid_path("name " + common::string_escape(name) + " is invalid", -1);

    uint64_t& new_entry = location->list[name];
    if (new_entry != 0)
        throw already_exists("target location already has an item named " + common::string_escape(name));
    uint64_t new_res_id = _resid_new();
    new_entry = new_res_id;

    bool resource_created = false;
    try {
        res_cnt& new_item = _items[new_res_id];
        if (new_item.ptr != nullptr)
            throw internal_error("failed to claim a resource id for the new item", log);
        resource_created = true;

        new_item.make_item(
            item.type,
            item.display_name,
            item.description,
            provider_id,
            item.params,
            item.returns,
            item.examples
        );

        return std::pair<uint64_t, res_cnt&>(new_res_id, new_item);
    } catch (...) {
        location->list.erase(name);
        if (resource_created)
            _items.erase(new_res_id);
        throw;
    }
}

uint64_t system::provider_item_add(uint64_t provider_id,
                                   uint64_t target_location,
                                   const std::string& name,
                                   const item_info& item) {
    std::lock_guard<std::mutex> guard(_lock);
    res_item_category* location = _get_category(target_location);

    if (provider_id == 0) {
        // Registering new provider
        if (target_location != STRTB_EVENT_ROOT)
            throw internal_error("tried to create a provider in a different location than root", log);
        if (item.type != ITEM_CATEGORY)
            throw internal_error("tried to create provider with wrong item type", log);
        provider_id = _resid_counter;
    } else if (provider_id != location->provider_id) {
        // Adding item for existing provider
        throw out_of_scope("target location does not belong to this provider");
    }

    return _provider_item_add(provider_id, location, name, item).first;
}

uint64_t system::provider_item_add(uint64_t provider_id,
                                   const item_path& target_location,
                                   const std::string& name,
                                   const item_info& item) {
    std::lock_guard<std::mutex> guard(_lock);

    if (provider_id == 0)
        throw internal_error("provider id was not specified", log);

    res_item_category* location = _get_category(provider_id, target_location);

    if (provider_id != location->provider_id)
        throw internal_error("category entry has a wrong provider id set", log);

    return _provider_item_add(provider_id, location, name, item).first;
}

void system::_provider_item_remove(res_item_category* location, const std::string& name) {
    uint64_t rid = 0;

    if (!item_path_validate_segment(name))
        throw invalid_path("name " + common::string_escape(name) + " is invalid", -1);

    // Delete entry in category
    auto cat_entry = location->list.find(name);
    if (cat_entry == location->list.end())
        throw not_found("target item " + common::string_escape(name) + " not found in this location");
    rid = cat_entry->second;
    location->list.erase(cat_entry);

    // Delete actual resource
    auto item = _items.find(rid);

    if (item == _items.end()) {
        log.put(logging::WARNING, {"Deleting entry ", common::string_escape(name),
                                   " that refers to an invalid resource ID of ", rid});
        return;
    }

    // If item is a subcategory, clear it recursively
    if (item->second.type() == ITEM_CATEGORY)
        _provider_category_clear(item->second.as_category());

    _items.erase(item);
}

void system::provider_item_remove(uint64_t provider_id, uint64_t target_location, const std::string& name) {
    std::lock_guard<std::mutex> guard(_lock);
    res_item_category* location = _get_category(target_location);

    if (provider_id == 0 && target_location != STRTB_EVENT_ROOT) // Unregistering provider
        throw internal_error("tried to remove provider without targetting root", log);

    if (provider_id != location->provider_id)
        throw out_of_scope("target location does not belong to this provider");

    _provider_item_remove(location, name);
}

void system::provider_item_remove(uint64_t provider_id, const item_path& target_location, const std::string& name) {
    std::lock_guard<std::mutex> guard(_lock);

    if (provider_id == 0)
        throw internal_error("provider id was not specified", log);

    res_item_category* location = _get_category(provider_id, target_location);

    if (provider_id != location->provider_id)
        throw internal_error("category entry has a wrong provider id set", log);

    _provider_item_remove(location, name);
}

void system::_provider_category_clear(res_item_category* location) {
    // Delete all held resources
    for (auto& entry : location->list) {
        auto item = _items.find(entry.second);

        // Make sure the item actually exists
        if (item == _items.end()) {
            log.put(logging::WARNING, {"Deleting entry ", common::string_escape(entry.first),
                                       " that refers to an invalid resource ID of ", entry.second});
            continue;
        }

        // If item is a subcategory, clear it recursively
        if (item->second.type() == ITEM_CATEGORY)
            _provider_category_clear(item->second.as_category());

        // Delete resource
        _items.erase(item);
    }

    // Delete all entries in the category
    location->list.clear();
}

void system::provider_category_clear(uint64_t provider_id, uint64_t target_location) {
    std::lock_guard<std::mutex> guard(_lock);

    if (provider_id == 0)
        throw internal_error("provider id was not specified", log);

    res_item_category* location = _get_category(target_location);

    if (provider_id != location->provider_id)
        throw out_of_scope("target location does not belong to this provider");

    _provider_category_clear(location);
}

void system::provider_category_clear(uint64_t provider_id, const item_path& target_location) {
    std::lock_guard<std::mutex> guard(_lock);

    if (provider_id == 0)
        throw internal_error("provider id was not specified", log);

    res_item_category* location = _get_category(provider_id, target_location);

    if (provider_id != location->provider_id)
        throw internal_error("category entry has a wrong provider id set", log);

    _provider_category_clear(location);
}

void system::_info(uint64_t resource_id, item_info& item) {
    auto& ref = _items.at(resource_id);
    if (ref.type() == ITEM_UNDEFINED)
        throw internal_error("resource container has undefined type", log);
    item.provider_id = ref.ptr->provider_id;
    item.type = ref.ptr->type;
    item.display_name = ref.ptr->display_name;
    item.description = ref.ptr->description;

    switch (ref.type()) {
    case ITEM_EVENT_SRC: {
        res_item_event_src* p = ref.as_event_src();
        item.params.push_back(p->param);
        item.returns = p->returns;
        item.examples = p->examples;
    }
    break;

    case ITEM_ACTION_SINK: {
        res_item_action_sink* p = ref.as_action_sink();
        item.params = p->params;
        item.returns = p->returns;
        item.examples = p->examples;
    }
    break;

    default:
        break;
    }
}

item_info system::info(uint64_t resource_id) {
    std::lock_guard<std::mutex> guard(_lock);

    try {
        item_info item;
        _info(resource_id, item);
        return item;
    } catch (std::out_of_range&) {
        throw not_found("item not found");
    }
}

item_listing system::info(const item_path& path) {
    std::lock_guard<std::mutex> guard(_lock);
    uint64_t rid = _follow_path(STRTB_EVENT_ROOT, path);

    try {
        item_listing item;
        item.resource_id = rid;
        if (!path.empty())
            item.name = path.back();
        _info(rid, item);
        return item;
    } catch (std::out_of_range&) {
        throw internal_error("category entry has an invalid resource id", log);
    }
}

std::vector<item_listing> system::_list(res_item_category* location) {
    std::vector<item_listing> items;
    items.reserve(location->list.size());

    for (const auto& i : location->list) {
        items.emplace_back();
        item_listing& item = items.back();
        item.name = i.first;
        item.resource_id = i.second;
        _info(i.second, item);
    }

    return items;
}

std::vector<item_listing> system::list(uint64_t resource_id) {
    std::lock_guard<std::mutex> guard(_lock);
    res_item_category* location = _get_category(resource_id);
    return _list(location);
}

std::vector<item_listing> system::list(const item_path& path) {
    std::lock_guard<std::mutex> guard(_lock);
    res_item_category* location = _get_category(STRTB_EVENT_ROOT, path);
    return _list(location);
}

void system::_provider_import(uint64_t provider_id, res_item_category* location, const json::value_object* entries) {
    for (auto entry = entries->begin(); entry != entries->end(); entry++) {
        const std::string& name = entry->first;
        json::value* item_def_value = entry->second;
        try {
            try {
                try {
                    const json::value_object* item_def = json::cast_object(item_def_value);
                    item_info item(item_def);
                    auto new_res = _provider_item_add(provider_id, location, name, item);

                    try {
                        if (item.type == ITEM_CATEGORY) {
                            // Recursively add all category entries, if specified
                            try {
                                const json::value_object* sub_entries = json::cast_object(&item_def->at("entries"));
                                _provider_import(provider_id, new_res.second.as_category(), sub_entries);
                            } catch (std::out_of_range&) {  // ignored, it's okay to not specify sub-items
                            } catch (json::wrong_type&) {
                                throw parsing_error("\"entries\" must be an object");
                            }
                        }
                    } catch (...) {
                        _provider_item_remove(location, name);
                        throw;
                    }
                } catch (json::wrong_type&) {
                    throw parsing_error("item definition must be an object");
                }
            } catch (parsing_error& e) {
                throw parsing_error("item " + common::string_escape(name) + ": " + e.what());
            }
        } catch (...) {     // Remove all added entries on exception
            // Removes all entries from first to last added (NOT the current one, as this caused the exception)
            while (entry != entries->begin()) {
                entry--;
                _provider_item_remove(location, entry->first);
            }
            throw;
        }
    }
}

void system::provider_import(uint64_t provider_id, uint64_t target_location, const json::value_object* entries) {
    std::lock_guard<std::mutex> guard(_lock);
    res_item_category* location = _get_category(target_location);

    if (provider_id != location->provider_id)
        throw out_of_scope("target location does not belong to this provider");

    _provider_import(provider_id, location, entries);
}

void system::provider_import(uint64_t provider_id, const item_path& target_location, const json::value_object* entries) {
    std::lock_guard<std::mutex> guard(_lock);

    if (provider_id == 0)
        throw internal_error("provider id was not specified", log);

    res_item_category* location = _get_category(provider_id, target_location);

    if (provider_id != location->provider_id)
        throw internal_error("category entry has a wrong provider id set", log);

    _provider_import(provider_id, location, entries);
}
