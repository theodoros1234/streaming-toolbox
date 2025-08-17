#ifndef STRTB_EVENT_PROVIDER_H
#define STRTB_EVENT_PROVIDER_H

#include "item.h"
#include <string>

namespace strtb::event {

class provider {
private:
    uint64_t _id = 0;
    std::string _name;

    void _setup_check();

public:
    provider();
    ~provider();
    void setup(const std::string& name, const std::string& display_name, const std::string& description);
    bool setup_finished();
    void teardown();
    uint64_t id() const;
    const std::string& name() const;

    uint64_t item_add(uint64_t target_location, const std::string& name, const item_info& item);
    uint64_t item_add(const item_path& target_location, const std::string& name, const item_info& item);
    void item_remove(uint64_t target_location, const std::string& name);
    void item_remove(const item_path& target_location, const std::string& name);
    uint64_t item_get_id(const item_path& target);  // TODO: make a bulk version of this
    void category_clear(uint64_t target_location);
    void category_clear(const item_path& target_location);
    void category_clear_root();
    void import(uint64_t target_location, const json::value_object* entries);
    void import(const item_path& target_location, const json::value_object* entries);
    void push_event(uint64_t target, const json::value* event);
    void push_event(uint64_t target, const json::value* event, bool filter);
    void push_event(uint64_t target, const json::value* event, int filter);
    void push_event(uint64_t target, const json::value* event, long filter);
    void push_event(uint64_t target, const json::value* event, long long filter);
    void push_event(uint64_t target, const json::value* event, unsigned int filter);
    void push_event(uint64_t target, const json::value* event, unsigned long filter);
    void push_event(uint64_t target, const json::value* event, unsigned long long filter);
    void push_event(uint64_t target, const json::value* event, const char* filter);
    void push_event(uint64_t target, const json::value* event, const std::string& filter);
    void push_event(uint64_t target, const json::value* event, const json::value* filter);
};

}

#endif // STRTB_EVENT_PROVIDER_H
