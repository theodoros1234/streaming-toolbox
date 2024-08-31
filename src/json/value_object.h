#ifndef STRTB_JSON_VALUE_OBJECT_H
#define STRTB_JSON_VALUE_OBJECT_H

#include "value.h"
#include "value_utils.h"

#include <map>
#include <string>
#include <vector>

namespace strtb::json {

class value_object : public value {
private:
    std::map<std::string, value*> _contents;
    typedef std::map<std::string, value*>::iterator iterator;
    typedef std::map<std::string, value*>::const_iterator const_iterator;
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
    std::vector<std::string> keys() const;
    bool exists(const std::string &key) const;
    value& at(const std::string &key) const;
    value* get(const std::string &key) const;
    void set(const std::string &key, val_type type);
    void set(const std::string &key, const value_auto &value);
    void set_move(const std::string &key, value* new_val);
    void set(iterator pos, val_type type);
    void set(iterator pos, const value_auto &value);
    void set_move(iterator pos, value* new_val);
    void erase(const std::string &key);
    void erase(iterator pos);
    iterator begin();
    const_iterator begin() const;
    iterator end();
    const_iterator end() const;
    virtual void write_to_stream(std::ostream &stream, int pretty_print, int pretty_print_level, const char* newline = "\n") const;
};

}

#endif // STRTB_JSON_VALUE_OBJECT_H
