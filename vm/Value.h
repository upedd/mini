#ifndef VALUE_H
#define VALUE_H
#include <string>

class Value {
public:
    enum class Type {
        BOOL,
        NIL,
        NUMBER,
    };

    Type type;
    union {
        bool boolean;
        double number;
    } as;


    bool is_bool() const;
    bool is_nil() const;
    bool is_number() const;

    bool as_bool() const;
    double as_number() const;

    static Value make_bool(bool value);
    static Value make_nil();
    static Value make_number(double value);

    std::string to_string() const;
};
#endif //VALUE_H
