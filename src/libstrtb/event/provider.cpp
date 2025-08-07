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
    if (_id == 0)
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

    _id = system_ptr->provider_item_add(0, STRTB_EVENT_ROOT, name, pcat);
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

uint64_t provider::id() const {
    return _id;
}

uint64_t provider::item_add(uint64_t target_location, const std::string &name, const item_info &item) {
    _setup_check();
    if (target_location == 0)
        target_location = _id;
    return system_ptr->provider_item_add(_id, target_location, name, item);
}

uint64_t provider::item_add(const item_path& target_location, const std::string &name, const item_info &item) {
    _setup_check();
    return system_ptr->provider_item_add(_id, target_location, name, item);
}

void provider::item_remove(uint64_t target_location, const std::string &name) {
    _setup_check();
    if (target_location == 0)
        target_location = _id;
    system_ptr->provider_item_remove(_id, target_location, name);
}

void provider::item_remove(const item_path& target_location, const std::string &name) {
    _setup_check();
    system_ptr->provider_item_remove(_id, target_location, name);
}

uint64_t provider::item_get_id(const item_path& target) {
    _setup_check();
    return system_ptr->provider_item_get_id(_id, target);
}

void provider::category_clear(uint64_t target_location) {
    _setup_check();
    system_ptr->provider_category_clear(_id, target_location);
}

void provider::category_clear(const item_path& target_location) {
    _setup_check();
    system_ptr->provider_category_clear(_id, target_location);
}

void provider::category_clear_root() {
    _setup_check();
    system_ptr->provider_category_clear(_id, _id);
}

void provider::import(uint64_t target_location, const json::value_object* entries) {
    _setup_check();
    system_ptr->provider_import(_id, target_location, entries);
}

void provider::import(const item_path& target_location, const json::value_object* entries) {
    _setup_check();
    system_ptr->provider_import(_id, target_location, entries);
}

void provider::push_event(uint64_t target, const json::value* event) {
    _setup_check();
    system_ptr->provider_push_event(_id, target, event);
}

void provider::push_event(uint64_t target, const json::value* event, bool filter) {
    _setup_check();
    system_ptr->provider_push_event(_id, target, event, filter);
}

void provider::push_event(uint64_t target, const json::value* event, long long filter) {
    _setup_check();
    system_ptr->provider_push_event(_id, target, event, filter);
}

void provider::push_event(uint64_t target, const json::value* event, const std::string& filter) {
    _setup_check();
    system_ptr->provider_push_event(_id, target, event, filter);
}

void provider::push_event(uint64_t target, const json::value* event, int filter) {
    push_event(target, event, (long long) filter);
}

void provider::push_event(uint64_t target, const json::value* event, long filter) {
    push_event(target, event, (long long) filter);
}

void provider::push_event(uint64_t target, const json::value* event, unsigned int filter) {
    push_event(target, event, (long long) filter);
}

void provider::push_event(uint64_t target, const json::value* event, unsigned long filter) {
    push_event(target, event, (long long) filter);
}

void provider::push_event(uint64_t target, const json::value* event, unsigned long long filter) {
    push_event(target, event, (long long) filter);
}

void provider::push_event(uint64_t target, const json::value* event, const char* filter) {
    push_event(target, event, std::string(filter));
}

void provider::push_event(uint64_t target, const json::value* event, const json::value* filter) {
    _setup_check();
    system_ptr->provider_push_event(_id, target, event, filter);
}
