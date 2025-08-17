#include "id_type.h"

using namespace strtb;
using namespace strtb::config;

id_type::id_type(int pos) : _pos(pos), _type(json::VAL_ARRAY) {}

id_type::id_type(long pos) : _pos(pos), _type(json::VAL_ARRAY) {}

id_type::id_type(long long pos) : _pos(pos), _type(json::VAL_ARRAY) {}

id_type::id_type(unsigned int pos) : _pos(pos), _type(json::VAL_ARRAY) {}

id_type::id_type(unsigned long pos) : _pos(pos), _type(json::VAL_ARRAY) {}

id_type::id_type(unsigned long long pos) : _pos(pos), _type(json::VAL_ARRAY) {}

id_type::id_type(const char *key) : _key(key), _type(json::VAL_OBJECT) {}

id_type::id_type(const std::string &key) : _key(key), _type(json::VAL_OBJECT) {}

json::val_type id_type::type() const {return _type;}

size_t id_type::pos() const {return _pos;}

const std::string& id_type::key() const {return _key;}
