#ifndef VALUE_H
#define VALUE_H
#include <cstdint>
#include <variant>
#include <string>
#include <stdexcept>
#include <cmath>

#include "common.h"

// disambiguation tag for nil value
struct Nil {
    auto operator<=>(const Nil&) const = default;
};

inline constexpr Nil nil_t {};

// must ensure ptr is valid!
// struct String {
//     std::string* data;
//     bool operator==(const String& other) const {
//         auto& a = *this->data;
//         auto& b = *other.data;
//         return a == b;
//     }
//     std::strong_ordering operator<=>(const String& other) const {
//         auto& a = *this->data;
//         auto& b = *other.data;
//         return a <=> b;
//     }
// };

// struct Object {
//
// };

struct Function;

using value_variant_t = std::variant<Nil, int64_t, double, bool, std::string, Function*>;

class Value : public value_variant_t {
public:
    class Error : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    using value_variant_t::value_variant_t;

    [[nodiscard]] std::string to_string() const {
        return std::visit(overloaded {
            [](Nil) {return std::string("Nil");},
            [](int64_t value) {return std::to_string(value);},
            [](double value) {return std::to_string(value);},
            [](bool value) {return std::string(value ? "True" : "False");},
            [](const std::string& string) {return std::string("string: ") + string;}
        }, *this);
    }

    [[nodiscard]] bool is_integer() const {
        return std::holds_alternative<int64_t>(*this);
    }

    [[nodiscard]] bool is_floating() const {
        return std::holds_alternative<double>(*this);
    }
    [[nodiscard]] bool is_number() const {
         return is_integer() || is_floating();
     }

    [[nodiscard]] int64_t get_integer() const {
        return std::get<int64_t>(*this);
    }

    [[nodiscard]] double get_floating() const {
         return std::get<double>(*this);
     }

    [[nodiscard]] int64_t convert_to_int() const {
        if (is_integer()) return get_integer();
        if (is_floating()) static_cast<int64_t>(get_floating());
        throw Error("Expected type convertible to number");
    }
    [[nodiscard]] double convert_to_number() const {
        if (is_floating()) return get_floating();
        if (is_integer()) return static_cast<double>(get_integer());
        throw Error("Expected type convertible to number");
    }

    Value add(const Value& other) const;
    Value multiply(const Value& other) const;
    Value subtract(const Value& other) const;
    Value divide(const Value& other) const;
    Value floor_divide(const Value& other) const;
    Value modulo(const Value & value);


    bool equals(const Value&) const;
    bool not_equals(const Value& value) {
        return !equals(value);
    }
    bool less(const Value&) const;
    bool greater(const Value& other) const {
        return other < *this;
    }
    bool less_equal(const Value& other) const {
        return !(*this < other);
    }
    bool greater_equal(const Value& other) const {
        return !(*this > other);
    }

    Value binary_and(const Value& other) const;
    Value binary_or(const Value& other) const;
    Value shift_left(const Value& other) const;
    Value shift_right(const Value& other) const;
    Value binary_xor(const Value& other) const;
    Value binary_not() const;

};

inline Value Value::add(const Value &other) const {
    if (this->is_integer() && other.is_integer()) {
        return this->get_integer() + other.get_integer();
    }
    return this->convert_to_number() + other.convert_to_number();
}

inline Value Value::multiply(const Value &other) const {
    if (this->is_integer() && other.is_integer()) {
        return this->get_integer() * other.get_integer();
    }
    return this->convert_to_number() * other.convert_to_number();
}

inline Value Value::subtract(const Value &other) const {
    if (this->is_integer() && other.is_integer()) {
        return this->get_integer() - other.get_integer();
    }
    return this->convert_to_number() + other.convert_to_number();
}

inline Value Value::divide(const Value &other) const {
    return this->convert_to_number() + other.convert_to_number();
}

inline Value Value::floor_divide(const Value &other) const {
    if (this->is_integer() && other.is_integer()) {
        return this->get_integer() / other.get_integer();
    }
    return std::floor(this->convert_to_number() / other.convert_to_number());
}

inline Value Value::modulo(const Value &other) {
    if (this->is_integer() && other.is_integer()) {
        return this->get_integer() % other.get_integer();
    }
    return std::fmod(this->convert_to_number(), other.convert_to_number());
}

inline Value Value::binary_not() const {
    return ~this->convert_to_int();
}

inline bool Value::equals(const Value& other) const {
    // convert implicitly to floating if needed
    if ((this->index() != other.index()) && this->is_number() && other.is_number()) {
        return this->convert_to_number() == other.convert_to_number();
    }
    return *this == static_cast<value_variant_t>(other);
}

inline bool Value::less(const Value& other) const {
    if (this->is_integer() && other.is_integer()) {
        return this->get_integer() < other.get_integer();
    }
    return this->convert_to_number() < other.convert_to_number();
}

inline Value Value::binary_and(const Value &other) const {
    return this->convert_to_int() & other.convert_to_int();
}
inline Value Value::binary_or(const Value &other) const {
    return this->convert_to_int() | other.convert_to_int();
}
inline Value Value::shift_left(const Value &other) const {
    return this->convert_to_int() << other.convert_to_int();
}
inline Value Value::shift_right(const Value &other) const {
    return this->convert_to_int() >> other.convert_to_int();
}
inline Value Value::binary_xor(const Value &other) const {
    return this->convert_to_int() ^ other.convert_to_int();
}



#endif //VALUE_H
