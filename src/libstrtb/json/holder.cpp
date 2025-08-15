#include "holder.h"
#include "cast.h"

using namespace strtb::json;


holder::holder() : _v(nullptr) {}

holder::holder(const holder& o) {
    if (o._v)
        _v = o.value()->copy();
}

holder::holder(holder&& o) {
    _v = o.detach(true);
}

holder::holder(strtb::json::value* v) : _v(v) {}

holder::holder(const value_auto& v) {
    if (!v.is_ptr() || v.value_as_ptr() != nullptr)
        _v = new_auto(v);
}

holder::holder(val_type new_type) {
    _v = new_default(new_type);
}

holder::~holder() {
    if (_v)
        delete _v;
}

holder& holder::operator=(json::value* other) {
    if (_v == other)
        return *this;

    set(other);
    return *this;
}

holder& holder::operator=(const json::holder& other) {
    if (_v)
        delete _v;
    _v = nullptr;

    if (other._v)
        _v = other._v->copy();

    return *this;
}

holder& holder::operator=(json::holder&& other) {
    if (_v)
        delete _v;
    _v = nullptr;

    _v = other._v;
    other._v = nullptr;

    return *this;
}

holder& holder::operator=(const value_auto& v) {
    if (_v)
        delete _v;
    _v = nullptr;

    if (!v.is_ptr() || v.value_as_ptr() != nullptr)
        _v = new_auto(v);

    return *this;
}

holder& holder::operator=(val_type new_type) {
    if (_v)
        delete _v;
    _v = nullptr;
    _v = new_default(new_type);
    return *this;
}

void holder::set(json::value* v) {
    if (_v)
        delete _v;
    _v = v;
}

void holder::clear() {
    if (_v)
        delete _v;
    _v = nullptr;
}

val_type holder::type() const {
    if (_v)
        return _v->type();
    else
        return VAL_UNDEFINED;
}

bool holder::empty() const {
    return _v == nullptr;
}

class value* holder::detach(bool ignore_undefined) {
    if (!_v && !ignore_undefined)
        throw undefined_exception();

    class value* tmp = _v;
    _v = nullptr;
    return tmp;
}

const strtb::json::value* holder::value(bool ignore_undefined) const {
    if (!_v && !ignore_undefined)
        throw undefined_exception();

    return _v;
}

const value_null* holder::as_null() const {
    return cast_null(_v);
}

const value_bool* holder::as_bool() const {
    return cast_bool(_v);
}

const value_int* holder::as_int() const {
    return cast_int(_v);
}

const value_float* holder::as_float() const {
    return cast_float(_v);
}

const value_string* holder::as_string() const {
    return cast_string(_v);
}

const value_array* holder::as_array() const {
    return cast_array(_v);
}

const value_object* holder::as_object() const {
    return cast_object(_v);
}

strtb::json::value* holder::value(bool ignore_undefined) {
    if (!_v && !ignore_undefined)
        throw undefined_exception();

    return _v;
}

value_null* holder::as_null() {
    return cast_null(_v);
}

value_bool* holder::as_bool() {
    return cast_bool(_v);
}

value_int* holder::as_int() {
    return cast_int(_v);
}

value_float* holder::as_float() {
    return cast_float(_v);
}

value_string* holder::as_string() {
    return cast_string(_v);
}

value_array* holder::as_array() {
    return cast_array(_v);
}

value_object* holder::as_object() {
    return cast_object(_v);
}
