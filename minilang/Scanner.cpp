//
// Created by MiłoszK on 19.06.2024.
//

#include "Scanner.h"

#include <iostream>

#include "Mini.h"

std::vector<Token> Scanner::scan_tokens() {
    while (!is_at_end()) {
        start = current;
        scan_token();
    }
    tokens.emplace_back(TokenType::END, "", std::any{}, line);
    return tokens;
}

bool Scanner::is_at_end() const {
    return current >= source.length();
}

void Scanner::string() {
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') line++;
        advance();
    }
    if (is_at_end()) {
        Mini::error(line, "unterminated string.");
    }
    // closing ".
    advance();

    std::string value = source.substr(start + 1, current - start - 2);
    add_token(TokenType::STRING, value);
}

bool Scanner::is_digit(const char c) {
    return c >= '0' && c <= '9';
}

char Scanner::peek_next() const {
    if (current + 1 >= source.length()) return '\0';
    return source[current + 1];
}

void Scanner::number() {
    while (is_digit(peek())) advance();
    if (peek() == '.' && is_digit(peek_next())) {
        advance();

        while (is_digit(peek())) advance();
    }
    add_token(TokenType::NUMBER, std::stod(source.substr(start, current - start)));
}

bool Scanner::is_alpha(const char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Scanner::is_alpha_numeric(const char c) {
    return is_alpha(c) || is_digit(c);
}

void Scanner::identifier() {
    while (is_alpha_numeric(peek())) advance();

    std::string text = source.substr(start, current - start);
    auto it = keywords.find(text);
    TokenType type = it != keywords.end() ? it->second : TokenType::IDENTIFIER;

    add_token(type);
}

void Scanner::scan_token() {
    char c = advance();

    switch (c) {
        case '(':
            add_token(TokenType::LEFT_PAREN);
            break;
        case ')':
            add_token(TokenType::RIGHT_PAREN);
            break;
        case '{':
            add_token(TokenType::LEFT_BRACE);
            break;
        case '}':
            add_token(TokenType::RIGHT_BRACE);
            break;
        case ',':
            add_token(TokenType::COMMA);
            break;
        case '.':
            add_token(TokenType::DOT);
            break;
        case '-':
            add_token(TokenType::MINUS);
            break;
        case '+':
            add_token(TokenType::PLUS);
            break;
        case ';':
            add_token(TokenType::SEMICOLON);
            break;
        case '*':
            add_token(TokenType::STAR);
            break;
        case '!':
            add_token(match('=') ? TokenType::BANG_EQUAL : TokenType::BANG);
            break;
        case '=':
            add_token(match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL);
            break;
        case '<':
            add_token(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS);
            break;
        case '>':
            add_token(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER);
            break;
        case '/':
            if (match('/')) {
                while (peek() != '\n' && !is_at_end()) advance();
            } else {
                add_token(TokenType::SLASH);
            }
            break;
        [[fallthrough]]
        case ' ':
        case '\r':
        case '\t':
            break;
        case '\n':
            line++;
            break;
        case '"':
            string();
            break;
        default:
            if (is_digit(c)) {
                number();
            } else if (is_alpha(c)) {
                identifier();
            } else {
                Mini::error(line, "Unexpected character");
            }
            break;
    }
}

char Scanner::advance() {
    return source[current++];
}

void Scanner::add_token(const TokenType type) {
    add_token(type, {});
}

void Scanner::add_token(TokenType type, const std::any &literal) {
    std::string text = source.substr(start, current - start);
    tokens.emplace_back(type, text, literal, line);
}

bool Scanner::match(const char expected) {
    if (is_at_end()) return false;
    if (source[current] != expected) return false;
    current++;
    return true;
}

char Scanner::peek() const {
    if (is_at_end()) return '\0';
    return source[current];
}
