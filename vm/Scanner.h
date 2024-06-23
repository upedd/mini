//
// Created by MiłoszK on 23.06.2024.
//

#ifndef SCANNER_H
#define SCANNER_H
#include <string_view>

#include "Token.h"


class Scanner {
public:
    explicit Scanner(const std::string_view source) : source(source) {}

    bool is_at_end() const;
    Token make_token(Token::Type type) const;
    Token error_token(std::string_view message) const;

    char peek_next();

    Token string();

    static bool is_digit(char c);

    Token number();

    bool is_alpha(char c);

    Token::Type check_keyword(std::string_view keyword, Token::Type type);

    Token::Type identifier_type();

    Token identifier();

    Token scan_token();
private:
    char advance();

    bool match(char expected);

    char peek() const;

    void skip_whitespace();

    std::string_view source;
    int current = 0;
    int start = 0;
    int line = 1;
};



#endif //SCANNER_H
