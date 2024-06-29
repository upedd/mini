#ifndef VALUE_H
#define VALUE_H
#include <cstdint>
#include <variant>
#include <string>

#include "common.h"


// disambiguation tag for nil value
struct Nil {
    explicit Nil() = default;
};

inline constexpr Nil nil_t {};

// must ensure ptr is valid!
struct String {
    std::string* data;
};

using Value = std::variant<Nil, int64_t, double, bool, String>;

// todo temp!
constexpr std::string value_to_string(Value v) {
    return std::visit(overloaded {
        [](Nil) {return std::string("Nil");},
        [](double value) {return std::to_string(value);},
        [](int64_t value) {return std::to_string(value);},
        [](bool value) {return std::string(value ? "True" : "False");},
        [](String string) {return std::string("string: ") + *string.data;}
    }, v);
}

#endif //VALUE_H
