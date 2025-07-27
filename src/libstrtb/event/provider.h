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
    uint64_t id();

    uint64_t item_add(uint64_t target_location, const std::string& name, const item_info& item);
    uint64_t item_add(const item_path& target_location, const std::string& name, const item_info& item);
    void item_remove(uint64_t target_location, const std::string& name);
    void item_remove(const item_path& target_location, const std::string& name);
    void category_clear(uint64_t target_location);
    void category_clear(const item_path& target_location);
    void import(uint64_t target_location, const json::value_object* entries);
    void import(const item_path& target_location, const json::value_object* entries);
};

}

#endif // STRTB_EVENT_PROVIDER_H
