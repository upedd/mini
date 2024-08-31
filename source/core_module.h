/**
* Defines classes needed for bite runtime
*/

#ifndef CORE_MODULE_H
#define CORE_MODULE_H
#include <optional>
#include <string>
#include <variant>

#include "shared/types.h"

// TODO: optimize!

//disambiguation tag for nil value
struct Nil {
    auto operator<=>(const Nil&) const = default;
};

struct Undefined {
    auto operator<=>(const Undefined&) const = default;
};

inline constexpr Nil nil_t {};

inline constexpr Undefined undefined {};

class Object;

using value_variant_t = std::variant<Nil, bite_int, bite_float, bool, Object*, std::string>;

class Value : public value_variant_t {
public:
    using value_variant_t::value_variant_t;
    [[nodiscard]] std::string to_string() const;

    template <typename T>
    [[nodiscard]] std::optional<T> as() const {
        if (is<T>()) {
            return get<T>();
        }
        return {};
    }

    template <typename T>
    [[nodiscard]] T get() const {
        return std::get<T>(*this);
    }

    template <typename T>
    [[nodiscard]] bool is() const {
        return std::holds_alternative<T>(*this);
    }
};

class VM;
class SharedContext;

inline void apply_core(VM* vm, SharedContext* context);
#endif //CORE_MODULE_H
