#ifndef COMMON_H
#define COMMON_H
#include <bitset>
#include <cassert>
#include <expected>

#include "types.h"

inline bool is_space(const char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

inline bool is_alpha(const char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline bool is_digit(const char c) {
    return c >= '0' && c <= '9';
}

inline bool is_alphanum(const char c) {
    return is_alpha(c) || is_digit(c);
}

inline bool is_identifier(const char c) {
    return is_alphanum(c) || c == '_';
}

inline char to_upper(const char c) {
    if (c >= 'a' && c <= 'z') {
        return static_cast<char>(c - 32);
    }
    return c;
}

inline bool is_hex_digit(const char c) {
    return is_digit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

inline bool is_binary_digit(const char c) {
    return c == '0' || c == '1';
}

inline bool is_octal_digit(const char c) {
    return c >= '0' && c <= '7';
}

inline bool is_number_literal_char(const char c) {
    return is_hex_digit(c) || c == '_' || c == 'x' || c == 'X' || c == 'p' || c == 'P';
}

inline int digit_to_int(const char c) {
    assert(is_digit(c));
    return c - '0';
}

inline int hex_digit_to_int(const char c) {
    assert(is_hex_digit(c));
    if (c >= '0' && c <= '9') {
        return digit_to_int(c);
    }
    return to_upper(c) - 'A' + 10;
}

// helper for std::visit
// source: https://en.cppreference.com/w/cpp/utility/variant/visit
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

// https://en.cppreference.com/w/cpp/experimental/scope_exit/scope_exit
template<class Fn>
class scope_exit {
public:
    explicit scope_exit(Fn&& fn) : fn(fn) {}
    ~scope_exit() { fn(); }
private:
    Fn fn;
};

template<typename T>
class bitflags {
public:
    // Inspiration: https://m-peko.github.io/craft-cpp/posts/different-ways-to-define-binary-flags/

    static_assert(std::is_enum_v<T>, "Flags can only be specialzed for enums.");

    bitflags() = default;

    explicit bitflags(unsigned long long value) : storage(value) {}

    // turn flag on
    constexpr bitflags& operator+=(const T& rhs) {
        storage.set(static_cast<std::size_t>(rhs));
        return *this; // return the result by reference
    }

    // turn flag off
    constexpr bitflags& operator-=(const T& rhs) {
        storage.reset(static_cast<std::size_t>(rhs));
        return *this; // return the result by reference
    }

    [[nodiscard]] constexpr bool operator[](const T& rhs) const {
        return storage.test(static_cast<std::size_t>(rhs));
    }

    [[nodiscard]] constexpr unsigned long long to_ullong() const {
        return storage.to_ullong();
    }
private:
    std::bitset<static_cast<std::size_t>(T::size)> storage;
};

// TODO: find better place for this!
enum class ClassAttributes {
    PRIVATE,
    OVERRIDE,
    ABSTRACT,
    GETTER,
    SETTER,
    OPERATOR,
    size // tracks ClassAttributes size. Must be at end!
};

#endif //COMMON_H
