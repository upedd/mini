#ifndef VALUE_H
#define VALUE_H
#include <cstdint>
#include <variant>
#include <string>
#include <stdexcept>
#include <cmath>
#include <optional>

#include "common.h"
#include "types.h"

// disambiguation tag for nil value
struct Nil {
    auto operator<=>(const Nil&) const = default;
};

inline constexpr Nil nil_t {};

class Object;

using value_variant_t = std::variant<Nil, bite_int, bite_float, bool, Object*, std::string>;

class Value : public value_variant_t {
public:
    class Error : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    using value_variant_t::value_variant_t;

    [[nodiscard]] std::string to_string() const;

    template<typename T>
    [[nodiscard]] std::optional<T> as() const {
        if (is<T>()) {
            return get<T>();
        }
        return {};
    }

    template<typename T>
    [[nodiscard]] T get() const {
        return std::get<T>(*this);
    }

    template<typename T>
    [[nodiscard]] bool is() const {
        return std::holds_alternative<T>(*this);
    }

    [[nodiscard]] bool is_falsey() const {
        if (is<bool>()) return !get<bool>();
        if (is<Nil>()) return true;
        return false;
    }

    [[nodiscard]] bool is_integer() const {
        return std::holds_alternative<bite_int>(*this);
    }

    [[nodiscard]] bool is_floating() const {
        return std::holds_alternative<bite_float>(*this);
    }
    [[nodiscard]] bool is_number() const {
         return is_integer() || is_floating();
     }

    [[nodiscard]] bite_int get_integer() const {
        return std::get<bite_int>(*this);
    }

    [[nodiscard]] bite_float get_floating() const {
         return std::get<bite_float>(*this);
     }

    [[nodiscard]] bite_int convert_to_int() const {
        if (is_integer()) return get_integer();
        if (is_floating()) static_cast<bite_int>(get_floating());
        throw Error("Expected type convertible to number");
    }
    [[nodiscard]] bite_float convert_to_number() const {
        if (is_floating()) return get_floating();
        if (is_integer()) return static_cast<bite_float>(get_integer());
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
    return this->convert_to_number() / other.convert_to_number();
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
