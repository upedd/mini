#ifndef COMMON_H
#define COMMON_H
#include <cassert>
#include <expected>
#include <optional>

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
    assert(is_alpha(c));
    if (c >= 'a' && c <= 'z') {
        static_cast<char>(c - 32);
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

class ConversionError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// todo: comments!

inline std::expected<bite_int, ConversionError> decimal_string_to_int(const std::string& string) {
    bite_int integer = 0;
    for (std::size_t i = 0; i <= string.size(); ++i) {
        if (string[i] == '_') continue;
        if (!is_digit(string[i])) {
            return std::unexpected(
                ConversionError(std::string("Expected decimal digit but got '") + string[i] + "'.")
                );
        }
        integer *= 10;
        integer += digit_to_int(string[i]);
    }
    return integer;
}

inline std::expected<bite_int, ConversionError> binary_string_to_int(const std::string& string) {
    bite_int integer = 0;
    for (std::size_t i = 2; i <= string.size(); ++i) {
        if (string[i] == '_') continue;
        if (!is_binary_digit(string[i])) {
            return std::unexpected(
                ConversionError(std::string("Expected binary digit but got '") + string[i] + "'.")
                );
        }
        integer *= 2;
        integer += digit_to_int(string[i]);
    }
    return integer;
}

inline std::expected<bite_int, ConversionError> hex_string_to_int(const std::string& string) {

    bite_int integer = 0;
    for (std::size_t i = 2; i <= string.size(); ++i) {
        if (string[i] == '_') continue;
        if (!is_hex_digit(string[i])) {
            return std::unexpected(
                ConversionError(std::string("Expected hex digit but got '") + string[i] + "'.")
                );
        }
        integer *= 16;
        integer += hex_digit_to_int(string[i]);
    }
    return integer;
}

inline std::expected<bite_int, ConversionError> octal_string_to_int(const std::string& string) {
    bite_int integer = 0;
    for (std::size_t i = 0; i <= string.size(); ++i) {
        if (string[i] == '_') continue;
        if (!is_octal_digit(string[i])) {
            return std::unexpected(
                ConversionError(std::string("Expected octal digit but got '") + string[i] + "'.")
                );
        }
        integer *= 8;
        integer += digit_to_int(string[i]);
    }
    return integer;
}


inline std::expected<bite_int, ConversionError> string_to_int(const std::string& string) {
    if (string[0] == '0') {
        if (to_upper(string[1]) == 'X') {
            return decimal_string_to_int(string);
        }
        if (to_upper(string[1]) == 'B') {
            return binary_string_to_int(string);
        }
        return octal_string_to_int(string);
    }
    return decimal_string_to_int(string);
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

#endif //COMMON_H
