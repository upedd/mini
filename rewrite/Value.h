#ifndef VALUE_H
#define VALUE_H
#include <cstdint>
#include <variant>
#include <string>

// disambiguation tag for nil value
struct Nil {
    explicit Nil() = default;
};

inline constexpr Nil nil_t {};

using Value = std::variant<Nil, int64_t, double, bool>;

template<class... Ts>
struct overloaded_temp : Ts... { using Ts::operator()...; };

// todo temp!
constexpr std::string value_to_string(Value v) {
    return std::visit(overloaded_temp {
        [](Nil value) {return std::string("Nil");},
            [](double value) {return std::to_string(value);},
                [](int64_t value) {return std::to_string(value);},
                    [](bool value) {return std::string(value ? "True" : "False");}
    }, v);
}

#endif //VALUE_H
