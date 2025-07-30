#include <stdexcept>
#include <cstdint>
#include <cassert>
#include "system.h"
#include "../logging/logging.h"
#include "../common/strescape.h"
#include "../json/cast.h"

using namespace strtb;
using namespace strtb::event;

event::system* strtb::event::system_ptr = nullptr;
static logging::source log("Event System");

system::res_path_follower::res_path_follower(const std::string& owner_name,
                                             const item_path& path,
                                             item_type wanted_type,
                                             uint64_t sub_rid) :
    owner_name(owner_name), path(path), wanted_type(wanted_type), sub_rid(sub_rid) {}

system::res_event_sub::res_event_sub(const item_path&path,
                                     const json::value* param,
                                     event_listener& listener,
                                     uint64_t rid) :
    path(listener._name, path, ITEM_EVENT_SRC, rid), listener(listener) {
    if (param)
        this->param = param->copy();
}

void system::res_item_category::path_follower_attach(res_path_follower* path_fl, uint64_t my_rid) {
    uint64_t old_target_rid = path_fl->target_rid;
    path_fl->path_pos_found++;
    path_fl->target_rid = my_rid;

    try {
        std::string& name = path_fl->path.at(path_fl->path_pos_found);

        // First check if we can pass this on to one of our entries
        if (!list.empty()) {
            auto itr = list.find(name);
            if (itr != list.end()) {
                try {
                    res_cnt& res = system_ptr->_items.at(itr->second);

                    if (path_fl->path_pos_found == path_fl->path.size() - 1) {
                        // Attaching to final piece
                        if (path_fl->wanted_type != res.type()) {
                            path_fl->status = PATH_FL_WRONG_TYPE;
                            path_fl->diagnostic_info = "target item has a different type than expected";
                        } else {
                            switch (res.type()) {
                            case ITEM_EVENT_SRC:
                                if (res.as_event_src()->sub_attach(*path_fl, itr->second))
                                    return;
                                break;

                            default:
                                throw internal_error("path follower is targetting an item type that isn't yet supported");
                            }
                        }
                    } else {
                        // Attaching to another subcategory inbetween
                        if (res.type() != ITEM_CATEGORY) {
                            path_fl->status = PATH_FL_WRONG_TYPE;
                            path_fl->diagnostic_info = "a category in this path was changed to another item type";
                        } else {
                            res.as_category()->path_follower_attach(path_fl, itr->second);
                            return;
                        }
                    }
                } catch (std::out_of_range&) {
                    throw internal_error("resource id " + std::to_string(itr->second) + " not found", log);
                }
            }
        }

        // If we don't have anyone to pass it to, hold it here for now.
        waiting_path_followers[name].insert(path_fl);
    } catch (std::out_of_range&) {
        path_fl->path_pos_found--;
        path_fl->target_rid = old_target_rid;
        throw internal_error("path follower's position went out of bounds");
    } catch (...) {
        path_fl->path_pos_found--;
        path_fl->target_rid = old_target_rid;
        throw;
    }
}

void system::res_item_category::path_follower_detach(res_path_follower* path_fl) {
    path_fl->target_rid = 0;
    auto itr = waiting_path_followers.find(path_fl->path[path_fl->path_pos_found]);

    if (itr == waiting_path_followers.end())
        throw internal_error("key not found for current path follower's position");

    if (itr->second.erase(path_fl) < 1)
        throw internal_error("couldn't find path follower");

    if (itr->second.empty())
        waiting_path_followers.erase(itr);
}

void system::res_item_category::path_follower_detach_all(std::set<res_path_follower*>& move_into,
                                                         uint64_t cat_rid) {
    for (auto& set : waiting_path_followers) {
        for (auto path_fl : set.second) {
            move_into.insert(path_fl);
            path_fl->path_pos_found--;
            path_fl->target_rid = cat_rid;
        }
    }
    waiting_path_followers.clear();
}

bool system::res_item_category::has_path_followers() {
    return !waiting_path_followers.empty();
}

bool system::res_item_event_src::sub_attach(res_path_follower &path_fl, uint64_t my_rid) {
    res_event_sub* sub_ptr = nullptr;
    assert(path_fl.wanted_type == ITEM_EVENT_SRC);

    // Find event sub resource
    try {
        sub_ptr = system_ptr->_event_subs.at(path_fl.sub_rid).get();
    } catch (std::out_of_range&) {
        throw internal_error("couldn't find event subscription with this resource id");
    }

    if (sub_ptr == nullptr)
        throw internal_error("resource id points to null event subscription");

    // Type checking
    json::val_type sub_param_type = sub_ptr->param.type();

    if (param.type == json::VAL_UNDEFINED &&
        sub_param_type != json::VAL_UNDEFINED) {    // param given but event source doesn't take any
        path_fl.status = PATH_FL_BAD_PARAM;
        path_fl.diagnostic_info = "event source doesn't take any parameters";
        return false;
    }

    if (param.required && sub_param_type == json::VAL_UNDEFINED) {  // param required but not given
        path_fl.status = PATH_FL_BAD_PARAM;
        path_fl.diagnostic_info = "parameter is required";
        return false;
    }

    if (param.type != json::VAL_UNDEFINED &&
        sub_param_type != json::VAL_UNDEFINED &&
        param.type != sub_param_type) {                             // params given but of wrong type
        path_fl.status = PATH_FL_BAD_PARAM;
        path_fl.diagnostic_info = "parameter of type " + json::type_to_string(param.type) + " needed, " +
                                  json::type_to_string(sub_param_type) + " given";
        return false;
    }

    // Attach event sub
    switch (sub_param_type) {
    case json::VAL_UNDEFINED:
        if (!subs_none.insert(sub_ptr).second)
            throw internal_error("event subscription is already attached");
        break;

    case json::VAL_BOOL:
        if (!subs_bool[sub_ptr->param.as_bool().value()].insert(sub_ptr).second)
            throw internal_error("event subscription appears to be already attached");
        break;

    case json::VAL_INT:
        if (!subs_int[sub_ptr->param.as_int().value()].insert(sub_ptr).second)
            throw internal_error("event subscription appears to be already attached");
        break;

    case json::VAL_STRING:
        if (!subs_string[sub_ptr->param.as_string().value()].insert(sub_ptr).second)
            throw internal_error("event subscription appears to be already attached");
        break;

    default:
        throw internal_error("event source and subscription are holding an unsupported param type");
    }

    path_fl.status = PATH_FL_READY;
    path_fl.target_rid = my_rid;
    path_fl.path_pos_found++;
    assert(path_fl.path_pos_found == path_fl.path.size());
    return true;
}

void system::res_item_event_src::sub_detach(res_event_sub* sub_ptr) {
    sub_ptr->path.target_rid = 0;
    switch (sub_ptr->param.type()) {
    case json::VAL_UNDEFINED: {
        if (subs_none.erase(sub_ptr) < 1)
            throw internal_error("couldn't find event subscription");
    }
    break;

    case json::VAL_BOOL: {
        if (subs_bool[sub_ptr->param.as_bool().value()].erase(sub_ptr) < 1)
            throw internal_error("couldn't find event subscription");
    }
    break;

    case json::VAL_INT: {
        auto itr = subs_int.find(sub_ptr->param.as_int().value());

        if (itr == subs_int.end())
            throw internal_error("couldn't find required key in event source");

        if (itr->second.erase(sub_ptr) < 1)
            throw internal_error("couldn't find event subscription");

        if (itr->second.empty())    // remove map entry if its set is empty
            subs_int.erase(itr);
    }
    break;

    case json::VAL_STRING: {
        auto itr = subs_string.find(sub_ptr->param.as_string().value());

        if (itr == subs_string.end())
            throw internal_error("couldn't find required key in event source");

        if (itr->second.erase(sub_ptr) < 1)
            throw internal_error("couldn't find event subscription");

        if (itr->second.empty())    // remove map entry if its set is empty
            subs_string.erase(itr);
    }
    break;

    default:
        throw internal_error("event subscription has an invalid parameter type");
    }
}

void system::res_item_event_src::sub_detach_all(std::set<res_path_follower*> &move_into, uint64_t cat_rid) {
    // Using a lambda to avoid copy-pasting this code many times
    static const auto detach = [](res_event_sub* sub, std::set<res_path_follower*>& move_into, uint64_t cat_rid) {
        move_into.insert(&sub->path);
        sub->path.status = PATH_FL_WAITING;
        sub->path.target_rid = cat_rid;
        sub->path.path_pos_found--;
    };

    for (auto sub : subs_none)
        detach(sub, move_into, cat_rid);
    subs_none.clear();

    for (size_t i=0; i<=1; i++) {
        for (auto sub : subs_bool[i])
            detach(sub, move_into, cat_rid);
        subs_bool[i].clear();
    }

    for (auto& set : subs_int)
        for (auto sub : set.second)
            detach(sub, move_into, cat_rid);
    subs_int.clear();

    for (auto& set : subs_string)
        for (auto sub : set.second)
            detach(sub, move_into, cat_rid);
    subs_string.clear();
}

bool system::res_item_event_src::has_subs() {
    return !(subs_none.empty() &&
             subs_bool[0].empty() &&
             subs_bool[1].empty() &&
             subs_int.empty() &&
             subs_string.empty());
}

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

std::pair<system::res_item_category*, uint64_t> system::_get_category(uint64_t start, const item_path& target_location) {
    try {
        uint64_t rid = _follow_path(start, target_location);
        return std::make_pair(_items.at(rid).as_category(), rid);
    } catch (std::out_of_range& e) {
        throw internal_error("category entry has an invalid resource id", log);
    }
}

system::res_item_event_src* system::_get_event_src(uint64_t target_location) {
    try {
        return _items.at(target_location).as_event_src();
    } catch (std::out_of_range&) {
        throw not_found("target location not found");
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

        // Forward any path followers to the new item
        auto path_fl_set_itr = location->waiting_path_followers.find(name);
        if (path_fl_set_itr != location->waiting_path_followers.end()) {
            std::vector<res_path_follower*> moved;

            try {
                for (auto path_fl : path_fl_set_itr->second) {
                    if (path_fl->path_pos_found == path_fl->path.size() - 1) {
                        // Attaching to final piece
                        if (path_fl->wanted_type != item.type) {
                            path_fl->status = PATH_FL_WRONG_TYPE;
                            path_fl->diagnostic_info = "target item has a different type than expected";
                        } else {
                            switch (item.type) {
                            case ITEM_EVENT_SRC:
                                if (new_item.as_event_src()->sub_attach(*path_fl, new_res_id))
                                    moved.push_back(path_fl);
                                break;

                            default:
                                throw internal_error("path follower is targetting an item type that isn't yet supported");
                            }
                        }
                    } else {
                        // Attaching to another subcategory inbetween
                        if (new_item.type() != ITEM_CATEGORY) {
                            path_fl->status = PATH_FL_WRONG_TYPE;
                            path_fl->diagnostic_info = "a category in this path was changed to another item type";
                        } else {
                            new_item.as_category()->path_follower_attach(path_fl, new_res_id);
                            moved.push_back(path_fl);
                        }
                    }
                }
            } catch (...) {
                // Remove all moved path followers before passing up the exception
                for (auto i : moved)
                    path_fl_set_itr->second.erase(i);
                throw;
            }

            // Remove all moved path followers
            for (auto i : moved)
                path_fl_set_itr->second.erase(i);

            // Remove set from map if empty
            if (path_fl_set_itr->second.empty())
                location->waiting_path_followers.erase(path_fl_set_itr);
        }

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

    auto [location, location_rid] = _get_category(provider_id, target_location);

    if (provider_id != location->provider_id)
        throw internal_error("category entry has a wrong provider id set", log);

    return _provider_item_add(provider_id, location, name, item).first;
}

void system::_provider_item_remove_path_followers(uint64_t location_rid,
                                                  res_item_category* location,
                                                  const std::string& name,
                                                  res_cnt& item) {
    // Make sure there's something to do first
    if (location->waiting_path_followers.count(name) ||
        (item.type() == ITEM_CATEGORY && item.as_category()->has_path_followers()) ||
        (item.type() == ITEM_EVENT_SRC && item.as_event_src()->has_subs())) {
        auto& path_fl_set = location->waiting_path_followers[name];

        // Clear error status of held back path followers
        for (auto path_fl : path_fl_set) {
            path_fl->status = PATH_FL_WAITING;
            path_fl->diagnostic_info.clear();
        }

        // Pull back path followers from deleted item
        switch (item.type()) {
        case ITEM_CATEGORY:
            item.as_category()->path_follower_detach_all(path_fl_set, location_rid);
            break;

        case ITEM_EVENT_SRC:
            item.as_event_src()->sub_detach_all(path_fl_set, location_rid);
            break;

        default:    // just makes clangd shut up
            break;
        }
    }
}

void system::_provider_item_remove(uint64_t location_rid, res_item_category* location, const std::string& name) {
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
        _provider_category_clear(location_rid, item->second.as_category());

    _provider_item_remove_path_followers(location_rid, location, name, item->second);
    _items.erase(item);
}

void system::provider_item_remove(uint64_t provider_id, uint64_t target_location, const std::string& name) {
    std::lock_guard<std::mutex> guard(_lock);
    res_item_category* location = _get_category(target_location);

    if (provider_id == 0 && target_location != STRTB_EVENT_ROOT) // Unregistering provider
        throw internal_error("tried to remove provider without targetting root", log);

    if (provider_id != location->provider_id)
        throw out_of_scope("target location does not belong to this provider");

    _provider_item_remove(target_location, location, name);
}

void system::provider_item_remove(uint64_t provider_id, const item_path& target_location, const std::string& name) {
    std::lock_guard<std::mutex> guard(_lock);

    if (provider_id == 0)
        throw internal_error("provider id was not specified", log);

    auto [location, location_rid] = _get_category(provider_id, target_location);

    if (provider_id != location->provider_id)
        throw internal_error("category entry has a wrong provider id set", log);

    _provider_item_remove(location_rid, location, name);
}

uint64_t system::provider_item_get_id(uint64_t provider_id, const item_path& target) {
    std::lock_guard<std::mutex> guard(_lock);

    if (provider_id == 0)
        throw internal_error("provider id was not specified", log);

    return _follow_path(provider_id, target);
}

void system::_provider_category_clear(uint64_t location_rid, res_item_category* location) {
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
            _provider_category_clear(location_rid, item->second.as_category());

        // Delete resource
        _provider_item_remove_path_followers(location_rid, location, entry.first, item->second);
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

    _provider_category_clear(target_location, location);
}

void system::provider_category_clear(uint64_t provider_id, const item_path& target_location) {
    std::lock_guard<std::mutex> guard(_lock);

    if (provider_id == 0)
        throw internal_error("provider id was not specified", log);

    auto [location, location_rid] = _get_category(provider_id, target_location);

    if (provider_id != location->provider_id)
        throw internal_error("category entry has a wrong provider id set", log);

    _provider_category_clear(location_rid, location);
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
        if (p->param.type != json::VAL_UNDEFINED)
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
    auto [location, location_rid] = _get_category(STRTB_EVENT_ROOT, path);
    return _list(location);
}

void system::_provider_import(uint64_t provider_id,
                              uint64_t location_rid,
                              res_item_category* location,
                              const json::value_object* entries) {
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
                                _provider_import(provider_id, location_rid, new_res.second.as_category(), sub_entries);
                            } catch (std::out_of_range&) {  // ignored, it's okay to not specify sub-items
                            } catch (json::wrong_type&) {
                                throw parsing_error("\"entries\" must be an object");
                            }
                        }
                    } catch (...) {
                        _provider_item_remove(location_rid, location, name);
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
                _provider_item_remove(location_rid, location, entry->first);
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

    _provider_import(provider_id, target_location, location, entries);
}

void system::provider_import(uint64_t provider_id, const item_path& target_location, const json::value_object* entries) {
    std::lock_guard<std::mutex> guard(_lock);

    if (provider_id == 0)
        throw internal_error("provider id was not specified", log);

    auto [location, location_rid] = _get_category(provider_id, target_location);

    if (provider_id != location->provider_id)
        throw internal_error("category entry has a wrong provider id set", log);

    _provider_import(provider_id, location_rid, location, entries);
}

uint64_t system::event_listener_subscribe(event_listener& listener,
                                          const item_path& event_source,
                                          const json::value* param) {
    std::lock_guard<std::mutex> guard(_lock);

    if (event_source.empty())
        throw out_of_scope("cannot subscribe to root category");

    if (param &&
        param->type() != json::VAL_BOOL &&
        param->type() != json::VAL_INT &&
        param->type() != json::VAL_STRING)
        throw wrong_type("param must be a nullptr, or it must be of types bool, int or string");

    uint64_t new_sub_id = _resid_new();
    std::unique_ptr<res_event_sub> new_sub(new res_event_sub(event_source, param, listener, new_sub_id));
    res_path_follower* path_fl = &new_sub->path;
    auto [new_sub_entry, added] = _event_subs.emplace(new_sub_id, std::move(new_sub));

    if (!added)
        throw internal_error("duplicate resource id found for event subscription");

    try {
        _items.at(STRTB_EVENT_ROOT).as_category()->path_follower_attach(path_fl, STRTB_EVENT_ROOT);
    } catch (...) {
        _event_subs.erase(new_sub_entry);
        throw;
    }

    return new_sub_id;
}

void system::_event_listener_unsubscribe(uint64_t subscription_id) {
    auto sub_itr = _event_subs.find(subscription_id);
    if (sub_itr == _event_subs.end())
        throw internal_error("event subscription not found");

    res_event_sub* sub = sub_itr->second.get();
    uint64_t remove_from = sub->path.target_rid;
    if (remove_from == 0) {
        _event_subs.erase(sub_itr);
        throw internal_error("event subscription was abandoned");   // maybe should just be a warning instead?
    }

    try {
        res_cnt& item = _items.at(remove_from);
        switch (item.type()) {
        case ITEM_CATEGORY:
            item.as_category()->path_follower_detach(&sub->path);
            _event_subs.erase(sub_itr);
            break;

        case ITEM_EVENT_SRC:
            item.as_event_src()->sub_detach(sub);
            _event_subs.erase(sub_itr);
            break;

        default:
            _event_subs.erase(sub_itr);
            throw internal_error("event subscription was held by an item of unsupported type");
        }
    } catch (std::out_of_range&) {
        _event_subs.erase(sub_itr);
        throw internal_error("event subscription was held by an item that no longer exists");
    }
}

void system::event_listener_unsubscribe(uint64_t subscription_id) {
    std::lock_guard<std::mutex> guard(_lock);
    _event_listener_unsubscribe(subscription_id);
}

void system::event_listener_unsubscribe(const std::set<uint64_t>& subscription_ids) {
    std::lock_guard<std::mutex> guard(_lock);
    for (auto sub_id : subscription_ids)
        _event_listener_unsubscribe(sub_id);
}

std::vector<item_info_path_follower> system::info_path_followers(uint64_t resource_id) {
    std::lock_guard<std::mutex> guard(_lock);
    res_item_category* category = _get_category(resource_id);

    std::vector<item_info_path_follower> info_returned;

    for (const auto& set : category->waiting_path_followers) {
        for (const auto path_fl : set.second) {
            item_info_path_follower i = {
                .owner_name = path_fl->owner_name,
                .sub_rid = path_fl->sub_rid,
                .path = path_fl->path,
                .wanted_type = path_fl->wanted_type,
                .status = path_fl->status,
                .diagnostic_info = path_fl->diagnostic_info
            };
            info_returned.push_back(std::move(i));
        }
    }

    return info_returned;
}

std::vector<item_info_event_sub> system::info_event_subs(uint64_t resource_id) {
    std::lock_guard<std::mutex> guard(_lock);
    res_item_event_src* event_src = _get_event_src(resource_id);

    std::vector<item_info_event_sub> info_returned;

    // using lambda to avoid copy-pasting this code many times
    static const auto add = [](res_event_sub* event_sub, std::vector<item_info_event_sub>& info_returned) {
        item_info_event_sub i = {
            .listener_name = event_sub->listener._name,
            .event_sub_rid = event_sub->path.sub_rid,
            .param = event_sub->param
        };
        info_returned.push_back(std::move(i));
    };

    for (const auto event_sub : event_src->subs_none)
        add(event_sub, info_returned);

    for (size_t i=0; i<1; i++)
        for (const auto event_sub : event_src->subs_bool[i])
            add(event_sub, info_returned);

    for (const auto& set : event_src->subs_int)
        for (const auto event_sub : set.second)
            add(event_sub, info_returned);

    for (const auto& set : event_src->subs_string)
        for (const auto event_sub : set.second)
            add(event_sub, info_returned);

    return info_returned;
}
