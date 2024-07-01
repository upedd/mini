#ifndef VALUE_H
#define VALUE_H
#include <cstdint>
#include <variant>
#include <string>
#include <stdexcept>

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

using value_variant_t = std::variant<Nil, int64_t, double, bool, std::string>;

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

    Value operator+(const Value& other) const;
    Value operator*(const Value& other) const;
    Value operator-(const Value& other) const;
    Value operator/(const Value& other) const;
    Value operator~() const;
    bool operator==(const Value&) const;
    bool operator<(const Value&) const;
    bool operator>(const Value& other) const {
        return other < *this;
    }
    bool operator>=(const Value& other) const {
        return !(*this < other);
    }
    bool operator<=(const Value& other) const {
        return !(*this > other);
    }

    Value operator&(const Value& other) const;
    Value operator|(const Value& other) const;
    Value operator<<(const Value& other) const;
    Value operator>>(const Value& other) const;
    Value operator^(const Value& other) const;
};

inline Value Value::operator+(const Value &other) const {
    if (this->is_integer() && other.is_integer()) {
        return this->get_integer() + other.get_integer();
    }
    return this->convert_to_number() + other.convert_to_number();
}

inline Value Value::operator*(const Value &other) const {
    if (this->is_integer() && other.is_integer()) {
        return this->get_integer() * other.get_integer();
    }
    return this->convert_to_number() * other.convert_to_number();
}

inline Value Value::operator-(const Value &other) const {
    if (this->is_integer() && other.is_integer()) {
        return this->get_integer() - other.get_integer();
    }
    return this->convert_to_number() + other.convert_to_number();
}

inline Value Value::operator/(const Value &other) const {
    return this->convert_to_number() + other.convert_to_number();
}

inline Value Value::operator~() const {
    return ~this->convert_to_int();
}

inline bool Value::operator==(const Value& other) const {
    // convert implicitly to floating if needed
    if ((this->index() != other.index()) && this->is_number() && other.is_number()) {
        return this->convert_to_number() == other.convert_to_number();
    }
    return *this == static_cast<value_variant_t>(other);
}

inline bool Value::operator<(const Value& other) const {
    if (this->is_integer() && other.is_integer()) {
        return this->get_integer() < other.get_integer();
    }
    return this->convert_to_number() < other.convert_to_number();
}

inline Value Value::operator&(const Value &other) const {
    return this->convert_to_int() & other.convert_to_int();
}
inline Value Value::operator|(const Value &other) const {
    return this->convert_to_int() | other.convert_to_int();
}
inline Value Value::operator<<(const Value &other) const {
    return this->convert_to_int() << other.convert_to_int();
}
inline Value Value::operator>>(const Value &other) const {
    return this->convert_to_int() >> other.convert_to_int();
}
inline Value Value::operator^(const Value &other) const {
    return this->convert_to_int() ^ other.convert_to_int();
}


#endif //VALUE_H
