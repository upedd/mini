#ifndef VALUE_H
#define VALUE_H
#include <string>

class Object {
public:
    enum class Type {
        STRING,
    };

    Type type;

    //static Object* allocate(size_t size, Type type);
};

class ObjectString : public Object {
public:
    std::string string;

    static ObjectString* allocate(const std::string& string);
};

class Value {
public:
    enum class Type {
        BOOL,
        NIL,
        NUMBER,
        OBJECT
    };

    Type type;
    union {
        bool boolean;
        double number;
        Object* object;
    } as;


    bool is_bool() const;
    bool is_nil() const;
    bool is_number() const;
    bool is_object() const;
    bool is_string() const;

    bool as_bool() const;
    double as_number() const;
    Object* as_object() const;
    ObjectString* as_string() const;
    char* as_cstring() const;

    static Value make_bool(bool value);
    static Value make_nil();
    static Value make_number(double value);
    static Value make_object(Object* object);

    std::string to_string() const;
private:
    bool is_object_type(Object::Type type) const;
};
#endif //VALUE_H
