#include "holder.h"

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

void holder::set(json::value* v) {
    if (_v)
        delete _v;
    _v = v;
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

const value_null& holder::as_null() const {
    if (!_v)
        throw undefined_exception();
    if (_v->type() != VAL_NULL)
        throw invalid_type();

    return *((const value_null*) _v);
}

const value_bool& holder::as_bool() const {
    if (!_v)
        throw undefined_exception();
    if (_v->type() != VAL_BOOL)
        throw invalid_type();

    return *((const value_bool*) _v);
}

const value_int& holder::as_int() const {
    if (!_v)
        throw undefined_exception();
    if (_v->type() != VAL_INT)
        throw invalid_type();

    return *((const value_int*) _v);
}

const value_float& holder::as_float() const {
    if (!_v)
        throw undefined_exception();
    if (_v->type() != VAL_FLOAT)
        throw invalid_type();

    return *((const value_float*) _v);
}

const value_string& holder::as_string() const {
    if (!_v)
        throw undefined_exception();
    if (_v->type() != VAL_STRING)
        throw invalid_type();

    return *((const value_string*) _v);
}

const value_array& holder::as_array() const {
    if (!_v)
        throw undefined_exception();
    if (_v->type() != VAL_ARRAY)
        throw invalid_type();

    return *((const value_array*) _v);
}

const value_object& holder::as_object() const {
    if (!_v)
        throw undefined_exception();
    if (_v->type() != VAL_OBJECT)
        throw invalid_type();

    return *((const value_object*) _v);
}

strtb::json::value* holder::value(bool ignore_undefined) {
    if (!_v && !ignore_undefined)
        throw undefined_exception();

    return _v;
}

value_null& holder::as_null() {
    if (!_v)
        throw undefined_exception();
    if (_v->type() != VAL_NULL)
        throw invalid_type();

    return *((value_null*) _v);
}

value_bool& holder::as_bool() {
    if (!_v)
        throw undefined_exception();
    if (_v->type() != VAL_BOOL)
        throw invalid_type();

    return *((value_bool*) _v);
}

value_int& holder::as_int() {
    if (!_v)
        throw undefined_exception();
    if (_v->type() != VAL_INT)
        throw invalid_type();

    return *((value_int*) _v);
}

value_float& holder::as_float() {
    if (!_v)
        throw undefined_exception();
    if (_v->type() != VAL_FLOAT)
        throw invalid_type();

    return *((value_float*) _v);
}

value_string& holder::as_string() {
    if (!_v)
        throw undefined_exception();
    if (_v->type() != VAL_STRING)
        throw invalid_type();

    return *((value_string*) _v);
}

value_array& holder::as_array() {
    if (!_v)
        throw undefined_exception();
    if (_v->type() != VAL_ARRAY)
        throw invalid_type();

    return *((value_array*) _v);
}

value_object& holder::as_object() {
    if (!_v)
        throw undefined_exception();
    if (_v->type() != VAL_OBJECT)
        throw invalid_type();

    return *((value_object*) _v);
}
