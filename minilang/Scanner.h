//
// Created by MiłoszK on 19.06.2024.
//

#ifndef SCANNER_H
#define SCANNER_H
#include <string>
#include <unordered_map>

#include "Token.h"


class Scanner {
public:
    explicit Scanner(const std::string_view source) : source(source) {}

    std::vector<Token> scan_tokens();
private:
    [[nodiscard]] bool is_at_end() const;

    void string();

    static bool is_digit(char c);

    [[nodiscard]] char peek_next() const;

    void number();

    static bool is_alpha(char c);

    static bool is_alpha_numeric(char c);

    void identifier();

    void scan_token();
    char advance();
    void add_token(TokenType type);
    void add_token(TokenType type, const std::any& literal);
    bool match(char expected);
    [[nodiscard]] char peek() const;

    const std::string source;
    std::vector<Token> tokens;
    int start = 0;
    int current = 0;
    int line = 1;

    std::unordered_map<std::string, TokenType> keywords = {
        {"and", TokenType::AND},
        {"class", TokenType::CLASS},
        {"else", TokenType::ELSE},
        {"false", TokenType::FALSE},
        {"for", TokenType::FOR},
        {"fun", TokenType::FUN},
        {"if", TokenType::IF},
        {"nil", TokenType::NIL},
        {"or", TokenType::OR},
        {"print", TokenType::PRINT},
        {"return", TokenType::RETURN},
        {"super", TokenType::SUPER},
        {"this", TokenType::THIS},
        {"true", TokenType::TRUE},
        {"var", TokenType::VAR},
        {"while", TokenType::WHILE}
    };
};



#endif //SCANNER_H
