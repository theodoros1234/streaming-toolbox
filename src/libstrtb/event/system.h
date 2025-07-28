#ifndef STRTB_EVENT_SYSTEM_H
#define STRTB_EVENT_SYSTEM_H

#include "item.h"
#include "provider.h"
#include "event_listener.h"

#include <map>
#include <vector>
#include <mutex>
#include <string>

#define STRTB_EVENT_ROOT 1

namespace strtb::event {

class system {
private:
    struct res_item {
        item_type type = ITEM_UNDEFINED;
        std::string display_name, description;
        uint64_t provider_id = 0;
        // TODO: maybe a resource string for an icon for better GUI appearance?
        virtual ~res_item() = default;
    };

    struct res_item_category : public res_item {
        std::map<std::string, uint64_t> list;
    };

    struct res_item_event_src : public res_item {
        std::vector<param_definition> params, returns;
        std::vector<example_definition> examples;
        // TODO: list of event listeners
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
    uint64_t _resid_counter = STRTB_EVENT_ROOT + 1;

    uint64_t _resid_new();
    uint64_t _follow_path(uint64_t start, const item_path& path);
    res_item_category* _get_category(uint64_t target_location);
    res_item_category* _get_category(uint64_t start, const item_path& target_location);
    std::pair<uint64_t, res_cnt&> _provider_item_add(uint64_t provider_id,
                                                     res_item_category* location,
                                                     const std::string& name,
                                                     const item_info& item);
    void _provider_item_remove(res_item_category* location, const std::string& name);
    void _provider_category_clear(res_item_category* location);
    void _provider_import(uint64_t provider_id, res_item_category* location, const json::value_object* entries);
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
