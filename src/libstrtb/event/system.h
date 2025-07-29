#ifndef STRTB_EVENT_SYSTEM_H
#define STRTB_EVENT_SYSTEM_H

#include "item.h"
#include "provider.h"
#include "event_listener.h"

#include <map>
#include <vector>
#include <mutex>
#include <string>
#include <memory>

#define STRTB_EVENT_ROOT 1

namespace strtb::event {

class system {
private:
    // used by event subs (and more later) to locate an item that hasn't been registered yet by its provider
    enum res_path_follower_status {
        PATH_FL_UNDEFINED,  // must be changed immediately after creation by the event system
        PATH_FL_READY,      // resource is attached to its wanted target
        PATH_FL_WAITING,    // resource is attached to a category waiting for its wanted target
        PATH_FL_WRONG_TYPE, // resource cannot attach to its wanted target cause it was a different item type
        PATH_FL_BAD_PARAM   // resource cannot attach to its wanted target due to having wrong parameters
    };

    struct res_path_follower {
        item_path path;
        size_t path_pos_found = 0;  // position of path segment it's currently looking for, path.size() if found
        item_type wanted_type = ITEM_UNDEFINED;
        uint64_t sub_rid = 0, target_rid = 0;
        res_path_follower_status status = PATH_FL_UNDEFINED;
        std::string diagnostic_info;

        res_path_follower(const item_path& path, item_type wanted_type, uint64_t sub_rid);
    };

    struct res_event_sub {
        res_path_follower path;
        json::holder param;
        event_listener& listener;

        res_event_sub(const item_path& path, const json::value* param, event_listener& listener, uint64_t rid);
    };

    struct res_item {
        item_type type = ITEM_UNDEFINED;
        std::string display_name, description;
        uint64_t provider_id = 0;
        // TODO: maybe a resource string for an icon for better GUI appearance?
        virtual ~res_item() = default;
    };

    struct res_item_category : public res_item {
        std::map<std::string, uint64_t> list;
        std::map<std::string, std::set<res_path_follower*> > waiting_path_followers;

        void path_follower_attach(res_path_follower* path_fl, uint64_t my_rid);
        void path_follower_detach(res_path_follower* path_fl);
        void path_follower_detach_all(std::set<res_path_follower*>& move_into, uint64_t cat_rid);
        bool has_path_followers();
    };

    struct res_item_event_src : public res_item {
        param_definition param;
        std::vector<param_definition> returns;
        std::vector<example_definition> examples;

        // Subscribed event listeners, based on parameter type
        std::set<res_event_sub*> subs_none;
        std::set<res_event_sub*> subs_bool[2];
        std::map<long long, std::set<res_event_sub*> > subs_int;
        std::map<std::string, std::set<res_event_sub*> > subs_string;

        bool sub_attach(res_path_follower& path_fl, uint64_t my_rid);
        void sub_detach(res_event_sub* sub_ptr);
        void sub_detach_all(std::set<res_path_follower*>& move_into, uint64_t cat_rid);
        bool has_subs();
    };

    struct res_item_action_sink : public res_item {
        std::vector<param_definition> params, returns;
        std::vector<example_definition> examples;
        // TODO: list of action handlers (listeners?)
    };

    struct res_cnt {
        // Resource container
        res_item* ptr = nullptr;

        res_cnt() = default;
        res_cnt(res_item* ptr);
        // Copy constructors deleted, cause there should only ever be one instance of the resource held underneath.
        res_cnt(res_cnt&) = delete;
        res_cnt(const res_cnt&) = delete;
        res_cnt(res_cnt&& from);
        ~res_cnt();
        void make_item(item_type type,
                       const std::string& display_name,
                       const std::string& description,
                       uint64_t provider_id);
        void make_item(item_type type,
                       const std::string& display_name,
                       const std::string& description,
                       uint64_t provider_id,
                       const std::vector<param_definition>& params,
                       const std::vector<param_definition>& returns,
                       const std::vector<example_definition>& examples);
        item_type type() const;
        res_item_category* as_category() const;
        res_item_event_src* as_event_src() const;
        res_item_action_sink* as_action_sink() const;
    };

    std::mutex _lock;
    std::map<uint64_t, res_cnt> _items;
    std::map<uint64_t, std::unique_ptr<res_event_sub> > _event_subs;
    uint64_t _resid_counter = STRTB_EVENT_ROOT + 1;

    uint64_t _resid_new();
    uint64_t _follow_path(uint64_t start, const item_path& path);
    res_item_category* _get_category(uint64_t target_location);
    std::pair<res_item_category*, uint64_t> _get_category(uint64_t start, const item_path& target_location);
    std::pair<uint64_t, res_cnt&> _provider_item_add(uint64_t provider_id,
                                                     res_item_category* location,
                                                     const std::string& name,
                                                     const item_info& item);
    void _provider_item_remove_path_followers(uint64_t location_rid,
                                              res_item_category* location,
                                              const std::string& name,
                                              res_cnt& item);
    void _provider_item_remove(uint64_t location_rid, res_item_category* location, const std::string& name);
    void _provider_category_clear(uint64_t location_rid, res_item_category* location);
    void _provider_import(uint64_t provider_id,
                          uint64_t location_rid,
                          res_item_category* location,
                          const json::value_object* entries);
    void _event_listener_unsubscribe(uint64_t subscription_id);
    void _info(uint64_t resource_id, item_info& item);
    std::vector<item_listing> _list(res_item_category* location);

protected:
    friend provider;
    friend event_listener;

    uint64_t provider_item_add(uint64_t provider_id,
                               uint64_t target_location,
                               const std::string& name,
                               const item_info& item);
    uint64_t provider_item_add(uint64_t provider_id,
                               const item_path& target_location,
                               const std::string& name,
                               const item_info& item);
    void provider_item_remove(uint64_t provider_id, uint64_t target_location, const std::string& name);
    void provider_item_remove(uint64_t provider_id, const item_path& target_location, const std::string& name);
    // TODO: bulk add/remove
    void provider_category_clear(uint64_t provider_id, uint64_t target_location);
    void provider_category_clear(uint64_t provider_id, const item_path& target_location);
    void provider_import(uint64_t provider_id, uint64_t target_location, const json::value_object* entries);
    void provider_import(uint64_t provider_id, const item_path& target_location, const json::value_object* entries);

    uint64_t event_listener_subscribe(event_listener& listener,
                                      const item_path& event_source,
                                      const json::value* param);
    void event_listener_unsubscribe(uint64_t subscription_id);
    void event_listener_unsubscribe(const std::set<uint64_t>& subscription_ids);

public:
    system();
    ~system();
    item_info info(uint64_t resource_id);
    item_listing info(const item_path& path);
    std::vector<item_listing> list(uint64_t resource_id);
    std::vector<item_listing> list(const item_path& path);
};

extern system* system_ptr;

}

#endif // STRTB_EVENT_SYSTEM_H
