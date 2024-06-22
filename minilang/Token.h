//
// Created by MiłoszK on 19.06.2024.
//

#ifndef TOKEN_H
#define TOKEN_H
#include <any>
#include <string>

#include "TokenType.h"


class Token {
public:
    Token(const TokenType type, const std::string_view lexeme, std::any literal, const int line) :
        type(type), lexeme(lexeme), literal(std::move(literal)), line(line) {}

    [[nodiscard]] std::string to_string() const;

    const TokenType type;
    const std::string lexeme;
    const std::any literal;
    const int line;
};



#endif //TOKEN_H
