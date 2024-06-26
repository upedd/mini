#ifndef COMMON_H
#define COMMON_H

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


// helper for std::visit
// source: https://en.cppreference.com/w/cpp/utility/variant/visit
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

#endif //COMMON_H
