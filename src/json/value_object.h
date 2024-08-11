#ifndef VALUE_OBJECT_H
#define VALUE_OBJECT_H

#include <map>
#include <string>
#include "value.h"

namespace json {

class value_object : public value {
private:
    std::map<std::string, value*> _contents;
public:
    value_object();
    value_object(const std::map<std::string, value*> &contents);
    value_object(const value_object &from);
    virtual ~value_object();
    virtual value* copy() const;
    std::map<std::string, value*> contents() const;
    void set_contents(const std::map<std::string, value*> &contents);
    void clear();
    size_t size() const;
    value& at(const std::string &key) const;
    value* get(const std::string &key) const;
    void set(const std::string &key);
    void set(const std::string &key, const bool new_val);
    void set(const std::string &key, const long long new_val);
    void set(const std::string &key, const double new_val);
    void set(const std::string &key, const char* new_val);
    void set(const std::string &key, const std::string &new_val);
    void set(const std::string &key, const value* new_val);
    void erase(const std::string &key);
};

}

#endif // VALUE_OBJECT_H
