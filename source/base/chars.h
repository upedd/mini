#ifndef CHARS_H
#define CHARS_H
#include <cassert>

// TODO: switch to custom assert

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

#endif //CHARS_H
