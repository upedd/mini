//
// Created by MiłoszK on 23.06.2024.
//

#include "Scanner.h"


bool Scanner::is_at_end() const {
    return current >= source.size();
}

Token Scanner::make_token(Token::Type type) const {
    return {type, source.substr(start, current - start), line};
}

Token Scanner::error_token(std::string_view message) const {
    return {Token::Type::ERROR, message, line};
}

char Scanner::peek_next() {
    if (is_at_end()) return '\0';
    return source[current + 1];
}

Token Scanner::string() {
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') line++;
        advance();
    }
    if (is_at_end()) return error_token("Unterminated string.");

    advance();
    return make_token(Token::Type::STRING);
}

char Scanner::advance() {
    return source[current++];
}

bool Scanner::match(char expected) {
    if (is_at_end() || source[current] != expected) return false;
    current++;
    return true;
}

char Scanner::peek() const {
    return source[current];
}

void Scanner::skip_whitespace() {
    while (true) {
        char c = peek();
        switch(c) {
            case ' ':
            case '\t':
            case '\r':
                advance();
                break;
            case '\n':
                advance();
                line++;
                break;
            case '/':
                if (peek_next() == '/') {
                    while (peek() != '\n' && !is_at_end()) advance();
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

bool Scanner::is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool Scanner::is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

Token::Type Scanner::check_keyword(std::string_view keyword, Token::Type type) {
    if (keyword == source.substr(start, current - start)) {
        return type;
    }
    return Token::Type::IDENTIFIER;
}

Token::Type Scanner::identifier_type() {
    // investigate macros for that https://github.com/v8/v8/blob/e77eebfe3b747fb315bd3baad09bec0953e53e68/src/parsing/scanner.cc#L1643
    switch(source[start]) {
        case 'a': return check_keyword("and", Token::Type::AND);
        case 'c': return check_keyword("class", Token::Type::CLASS);
        case 'e': return check_keyword("else", Token::Type::ELSE);
        case 'i': return check_keyword("if", Token::Type::IF);
        case 'n': return check_keyword("nil", Token::Type::NIL);
        case 'o': return check_keyword("or", Token::Type::OR);
        case 'p': return check_keyword("print", Token::Type::PRINT);
        case 'r': return check_keyword("return", Token::Type::RETURN);
        case 's': return check_keyword("super", Token::Type::SUPER);
        case 'v': return check_keyword("var", Token::Type::VAR);
        case 'w': return check_keyword("while", Token::Type::WHILE);
        case 'f':
            if (current - start > 1) {
                switch (source[start + 1]) {
                    case 'a': return check_keyword("false", Token::Type::FALSE);
                    case 'o': return check_keyword("for", Token::Type::FOR);
                    case 'u': return check_keyword("fun", Token::Type::FUN);
                }
            }
            break;
        case 't':
            if (current - start > 1) {
                switch (source[start + 1]) {
                    case 'h': return check_keyword("this", Token::Type::THIS);
                    case 'r': return check_keyword("true", Token::Type::TRUE);
                }
            }
            break;
    }

    return Token::Type::IDENTIFIER;
}

Token Scanner::identifier() {
    while (is_alpha(peek()) || is_digit(peek())) advance();
    return make_token(identifier_type());
}


Token Scanner::number() {
    while (is_digit(peek())) advance();

    if (peek() == '.' && is_digit(peek_next())) {
        advance();

        while (is_digit(peek())) advance();
    }

    return make_token(Token::Type::NUMBER);
}


Token Scanner::scan_token() {
    skip_whitespace();
    start = current;
    if (is_at_end()) return make_token(Token::Type::END);

    char c = advance();

    if (is_alpha(c)) return identifier();
    if (is_digit(c)) return number();

    switch (c) {
        case '(': return make_token(Token::Type::LEFT_PAREN);
        case ')': return make_token(Token::Type::RIGHT_PAREN);
        case '{': return make_token(Token::Type::LEFT_BRACE);
        case '}': return make_token(Token::Type::RIGHT_BRACE);
        case ';': return make_token(Token::Type::SEMICOLON);
        case ',': return make_token(Token::Type::COMMA);
        case '.': return make_token(Token::Type::DOT);
        case '-': return make_token(Token::Type::MINUS);
        case '+': return make_token(Token::Type::PLUS);
        case '/': return make_token(Token::Type::SLASH);
        case '*': return make_token(Token::Type::STAR);
        case '!': return make_token(match('=') ? Token::Type::BANG_EQUAL : Token::Type::BANG);
        case '=': return make_token(match('=') ? Token::Type::EQUAL_EQUAL : Token::Type::EQUAL);
        case '<': return make_token(match('=') ? Token::Type::LESS_EQUAL : Token::Type::LESS);
        case '>': return make_token(match('=') ? Token::Type::GREATER_EQUAL : Token::Type::GREATER);
        case '"': return string();
    }

    return error_token("Unexpected character");
}
