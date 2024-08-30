#ifndef STRTB_CONFIG_ID_TYPE_H
#define STRTB_CONFIG_ID_TYPE_H

#include "../json/value.h"
#include <string>

namespace strtb::config {

class id_type {
private:
    size_t _pos;
    const std::string _key;
    json::val_type _type;
public:
    id_type(int pos);
    id_type(long pos);
    id_type(long long pos);
    id_type(unsigned int pos);
    id_type(unsigned long pos);
    id_type(unsigned long long pos);
    id_type(const char *key);
    id_type(const std::string &key);
    json::val_type type() const;
    size_t pos() const;
    const std::string& key() const;
};

}

#endif // STRTB_CONFIG_ID_TYPE_H
