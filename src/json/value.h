#ifndef VALUE_H
#define VALUE_H

namespace json {

enum val_type {VAL_NULL, VAL_BOOL, VAL_INT, VAL_FLOAT, VAL_STRING, VAL_ARRAY, VAL_OBJECT};

class value {
private:
    val_type _type;
protected:
    value(val_type type);
public:
    virtual value* copy() const = 0;
    virtual ~value() = default;
    val_type type() const;
};

}

#endif // VALUE_H
