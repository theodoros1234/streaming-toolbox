#ifndef VALUE_ARRAY_H
#define VALUE_ARRAY_H

#include "value.h"
#include <vector>
#include <string>

namespace json {

class value_array : public value {
private:
    std::vector<value*> _contents;
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
    void set(const size_t pos);
    void set(const size_t pos, const bool new_val);
    void set(const size_t pos, const long long new_val);
    void set(const size_t pos, const double new_val);
    void set(const size_t pos, const char* new_val);
    void set(const size_t pos, const std::string &new_val);
    void set(const size_t pos, const value* new_val);
    void set_move(const size_t pos, value* new_val);
    value& at_back();
    value* back();
    void push_back();
    void push_back(const bool new_val);
    void push_back(const long long new_val);
    void push_back(const double new_val);
    void push_back(const char* new_val);
    void push_back(const std::string &new_val);
    void push_back(const value* new_val);
    void push_back_move(value* new_val);
    void pop_back();
    void insert(const size_t pos);
    void insert(const size_t pos, const bool new_val);
    void insert(const size_t pos, const long long new_val);
    void insert(const size_t pos, const double new_val);
    void insert(const size_t pos, const char* new_val);
    void insert(const size_t pos, const std::string &new_val);
    void insert(const size_t pos, const value* new_val);
    void insert_move(const size_t pos, value* new_val);
    void erase(const size_t pos);
    virtual void write_to_stream(std::ostream &stream, int pretty_print, int pretty_print_level, const char* newline = "\n") const;
};

}

#endif // VALUE_ARRAY_H
