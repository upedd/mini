#ifndef VALUE_H
#define VALUE_H
#include <cassert>
#include <cstdint>
#include <variant>
#include <string>

#include "common.h"


// disambiguation tag for nil value
enum class Nil {
};

inline constexpr Nil nil_t {};

// must ensure ptr is valid!
struct String {
    std::string* data;
};

// using Value = std::variant<Nil, int64_t, double, bool, String>;

using number_variant_t = std::variant<int64_t, double>;

// TODO: wasting space!
/**
 * Holds hybrid of integer and double converting if needed
 */
class Number : public number_variant_t {
public:
    using number_variant_t::number_variant_t;
    [[nodiscard]] bool is_integer() const {
        return std::holds_alternative<int64_t>(*this);
    }

    [[nodiscard]] bool is_floating() const {
        return std::holds_alternative<double>(*this);
    }

    // converts if needed
    [[nodiscard]] int64_t get_integer() const {
        return std::get<int64_t>(*this);
    }

    // converts if needed
    [[nodiscard]] double get_floating() const {
        return std::get<double>(*this);
    }

    [[nodiscard]] int64_t convert_to_int() const {
        if (is_integer()) return get_integer();
        return static_cast<int64_t>(get_floating());
    }
    [[nodiscard]] double convert_to_number() const {
        if (is_floating()) return get_floating();
        if (is_integer()) return static_cast<double>(get_integer());
        return {};
    }

    [[nodiscard]] std::string to_string() const {
        return is_floating() ? std::to_string(get_floating()) : std::to_string(get_integer());
    }

    Number operator+(const Number& other) const;
    Number operator*(const Number& other) const;
    Number operator-(const Number& other) const;
    Number operator/(const Number& other) const;
};

inline Number Number::operator+(const Number &other) const {
    if (this->is_integer() && other.is_integer()) {
        return this->get_integer() + other.get_integer();
    }
    if (this->is_floating() && other.is_floating()) {
        return this->get_floating() + other.get_floating();
    }
    return this->convert_to_number() + other.convert_to_number();
}

inline Number Number::operator*(const Number &other) const {
    if (this->is_integer() && other.is_integer()) {
        return this->get_integer() * other.get_integer();
    }
    if (this->is_floating() && other.is_floating()) {
        return this->get_floating() * other.get_floating();
    }
    return this->convert_to_number() * other.convert_to_number();
}

inline Number Number::operator-(const Number &other) const {
    if (this->is_integer() && other.is_integer()) {
        return this->get_integer() - other.get_integer();
    }
    if (this->is_floating() && other.is_floating()) {
        return this->get_floating() - other.get_floating();
    }
    return this->convert_to_number() + other.convert_to_number();
}

inline Number Number::operator/(const Number &other) const {
    return this->convert_to_number() + other.convert_to_number();
}

using value_variant_t = std::variant<Nil, Number, bool, String>;

class Value : public value_variant_t {
public:
    using value_variant_t::value_variant_t;

     std::string to_string() {
        return std::visit(overloaded {
            [](Nil) {return std::string("Nil");},
            [](Number value) {return value.to_string();},
            [](bool value) {return std::string(value ? "True" : "False");},
            [](String string) {return std::string("string: ") + *string.data;}
        }, *this);
    }

    bool is_number() {
         return std::holds_alternative<Number>(*this);
     }

    Number get_number() {
         return std::get<Number>(*this);
     }
};

// todo temp!


#endif //VALUE_H
