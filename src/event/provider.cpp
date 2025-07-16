#include "provider.h"
#include "system.h"
#include <stdexcept>

using namespace strtb::event;

provider::provider() {}

provider::~provider() {
    if (_id)
        system_ptr->provider_item_remove(0, STRTB_EVENT_ROOT, _name);
}

void provider::_setup_check() {
    if (!_id)
        throw std::logic_error("event provider not set up");
}

void provider::setup(const std::string& name, const std::string& display_name, const std::string& description) {
    if (_id)
        throw std::logic_error("event provider already set up");

    item_info pcat;
    pcat.type = ITEM_CATEGORY;
    pcat.display_name = display_name;
    pcat.description = description;
    _name = name;

    system_ptr->provider_item_add(0, STRTB_EVENT_ROOT, name, pcat);
}

bool provider::setup_finished() {
    return _id;
}

void provider::teardown() {
    _setup_check();
    system_ptr->provider_item_remove(0, STRTB_EVENT_ROOT, _name);
    _id = 0;
    _name.clear();
}

uint64_t provider::id() {
    return _id;
}

uint64_t provider::item_add(uint64_t target_location, const std::string &name, const item_info &item) {
    _setup_check();
    return system_ptr->provider_item_add(_id, target_location, name, item);
}

uint64_t provider::item_add(const item_path& target_location, const std::string &name, const item_info &item) {
    _setup_check();
    return system_ptr->provider_item_add(_id, target_location, name, item);
}

void provider::item_remove(uint64_t target_location, const std::string &name) {
    _setup_check();
    system_ptr->provider_item_remove(_id, target_location, name);
}

void provider::item_remove(const item_path& target_location, const std::string &name) {
    _setup_check();
    system_ptr->provider_item_remove(_id, target_location, name);
}

void provider::category_clear(uint64_t resource_id) {
    _setup_check();
    system_ptr->provider_category_clear(_id, resource_id);
}

void provider::category_clear(const item_path& inner_location) {
    _setup_check();
    system_ptr->provider_category_clear(_id, inner_location);
}
