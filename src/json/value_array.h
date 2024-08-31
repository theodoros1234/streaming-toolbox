#ifndef STRTB_JSON_VALUE_ARRAY_H
#define STRTB_JSON_VALUE_ARRAY_H

#include "value.h"
#include "value_utils.h"

#include <vector>
#include <string>

namespace strtb::json {

class value_array : public value {
private:
    std::vector<value*> _contents;
    typedef std::vector<value*>::iterator iterator;
    typedef std::vector<value*>::const_iterator const_iterator;
public:
    value_array();
    value_array(const std::vector<value*> &contents);
    value_array(const value_array &from);
    virtual ~value_array();
    virtual value* copy() const;
    std::vector<value*> contents() const;
    void set_contents(const std::vector<value*> &contents);
    void clear();
    size_t size() const;
    value& at(const size_t pos) const;
    value* get(const size_t pos) const;
    void set(const size_t pos, val_type type);
    void set(const size_t pos, const value_auto &new_val);
    void set_move(const size_t pos, value* new_val);
    void set(iterator pos, val_type type);
    void set(iterator pos, const value_auto &new_val);
    void set_move(iterator pos, value* new_val);
    value& at_back();
    value* back();
    void push_back(val_type type);
    void push_back(const value_auto &new_val);
    void push_back_move(value* new_val);
    void pop_back();
    void insert(const size_t pos, val_type type);
    void insert(const size_t pos, const value_auto &new_val);
    void insert_move(const size_t pos, value* new_val);
    void insert(const_iterator pos, val_type type);
    void insert(const_iterator pos, const value_auto &new_val);
    void insert_move(const_iterator pos, value* new_val);
    void erase(const size_t pos);
    void erase(const_iterator pos);
    iterator begin();
    const_iterator begin() const;
    iterator end();
    const_iterator end() const;
    virtual void write_to_stream(std::ostream &stream, int pretty_print, int pretty_print_level, const char* newline = "\n") const;
};

}

#endif // STRTB_JSON_VALUE_ARRAY_H
