#include "Lexer.h"

#include <expected>

#include "common.h"

std::expected<Token, Lexer::Error> Lexer::next_token() {
    skip_whitespace();
    start_pos = current_pos;
    switch (char c = advance(); c) {
        case '\0':
            return make_token(Token::Type::END);
        case '{':
            return make_token(Token::Type::LEFT_BRACE);
        case '}':
            return make_token(Token::Type::RIGHT_BRACE);
        case '(':
            return make_token(Token::Type::LEFT_PAREN);
        case ')':
            return make_token(Token::Type::RIGHT_PAREN);
        case '[':
            return make_token(Token::Type::LEFT_BRACKET);
        case ']':
            return make_token(Token::Type::RIGHT_BRACKET);
        case ',':
            return make_token(Token::Type::COMMA);
        case ';':
            return make_token(Token::Type::SEMICOLON);
        case '~':
            return make_token(Token::Type::TILDE);
        case ':':
            return make_token(Token::Type::COLON);
        case '!':
            return make_token(match('=') ? Token::Type::BANG_EQUAL : Token::Type::BANG);
        case '+':
            return make_token(match('=') ? Token::Type::PLUS_EQUAL: Token::Type::PLUS);
        case '-':
            return make_token(match('=') ? Token::Type::MINUS_EQUAL : Token::Type::MINUS);
        case '*':
            return make_token(match('=') ? Token::Type::STAR_EQUAL : Token::Type::STAR);
        case '%':
            return make_token(match('=') ? Token::Type::PERCENT_EQUAL : Token::Type::PERCENT);
        case '^':
            return make_token(match('=') ? Token::Type::CARET_EQUAL : Token::Type::CARET);
        case '=':
            return make_token(match('=') ? Token::Type::EQUAL_EQUAL : Token::Type::EQUAL);
        case '&': {
            if (match('&')) {
                return make_token(Token::Type::AND_AND);
            }
            return make_token(match('=') ? Token::Type::AND_EQUAL : Token::Type::AND);
        }
        case '|': {
            if (match('|')) {
                return make_token(Token::Type::BAR_BAR);
            }
            return make_token(match('=') ? Token::Type::BAR_EQUAL : Token::Type::BAR);
        }
        case '/': {
            if (match('/')) {
                return make_token(match('=') ?Token::Type::SLASH_SLASH_EQUAL : Token::Type::SLASH_SLASH);
            }
            return make_token(match('=') ? Token::Type::SLASH_EQUAL : Token::Type::SLASH);
        }
        case '.': {
            if (match('.')) {
                return make_token(match('.') ? Token::Type::DOT_DOT_DOT : Token::Type::DOT_DOT);
            }
            return make_token(Token::Type::DOT);
        }
        case '<': {
            if (match('<')) {
                return make_token(match('=') ? Token::Type::LESS_LESS_EQUAL : Token::Type::LESS_LESS);
            }
            return make_token(match('=') ? Token::Type::LESS_EQUAL : Token::Type::LESS);
        }
        case '>': {
            if (match('>')) {
                return make_token(match('=') ? Token::Type::GREATER_GREATER_EQUAL : Token::Type::GREATER_GREATER);
            }
            return make_token(match('=') ? Token::Type::GREATER_EQUAL : Token::Type::GREATER);
        }
        case '"':
            return string();
        case '@':
            return label();
        default: {
            if (is_digit(c)) { // TODO: number literals should be able to start with dot
                return integer_or_number();
            }
            if (is_identifier(c)) {
                return keyword_or_identifier();
            }
            return make_error("Unexpected character.");
        }
    }
}

std::string_view Lexer::get_source() const {
    return source;
}

char Lexer::advance() {
    if (current_pos >= source.size()) return '\0';
    return source[current_pos++];
}

char Lexer::current() const {
    if (current_pos >= source.size()) return '\0';
    return source[current_pos];
}

bool Lexer::match(char c) {
    if (current() == c) {
        advance();
        return true;
    }
    return false;
}

bool Lexer::at_end() const {
    return current_pos >= source.size();
}

void Lexer::skip_whitespace() {
    while (true) {
        if (is_space(current()) || current() == '\n') {
            advance();
        } else if (current() == '#') {
            while (!at_end() && !match('\n')) advance();
        } else {
            break;
        }
    }
}

void Lexer::consume_identifier() {
    while (is_identifier(current())) {
        advance();
    }
}

Token Lexer::make_token(Token::Type type) const {
    return {.type = type, .source_offset = start_pos, .length = current_pos - start_pos};
}

std::unexpected<Lexer::Error> Lexer::make_error(const std::string& message) const {
    return std::unexpected<Error>({start_pos, message});
}

Token Lexer::keyword_or_identifier() {
    // maybe optimize like v8 https://github.com/v8/v8/blob/e77eebfe3b747fb315bd3baad09bec0953e53e68/src/parsing/scanner.cc#L1643
    // bench if worth additional complexity
    consume_identifier();
    int size = current_pos - start_pos;
    std::string_view current = source.substr(start_pos, size);

    if (current == "class") {
        return make_token(Token::Type::CLASS);
    } else if (current == "fun") {
        return make_token(Token::Type::FUN);
    } else if (current == "return") {
        return make_token(Token::Type::RETURN);
    } else if (current == "if") {
        return make_token(Token::Type::IF);
    } else if (current == "is") {
        return make_token(Token::Type::IS);
    } else if (current == "in") {
        return make_token(Token::Type::IN);
    } else if (current == "break") {
        return make_token(Token::Type::BREAK);
    } else if (current == "continue") {
        return make_token(Token::Type::CONTINUE);
    } else if (current == "match") {
        return make_token(Token::Type::MATCH);
    } else if (current == "true") {
        return make_token(Token::Type::TRUE);
    } else if (current == "false") {
        return make_token(Token::Type::FALSE);
    } else if (current == "else") {
        return make_token(Token::Type::ELSE);
    } else if (current == "this") {
        return make_token(Token::Type::THIS);
    } else if (current == "loop") {
        return make_token(Token::Type::LOOP);
    } else if (current == "super") {
        return make_token(Token::Type::SUPER);
    } else if (current == "nil") {
        return make_token(Token::Type::NIL);
    } else if (current == "let") {
        return make_token(Token::Type::LET);
    } else if (current == "while") {
        return make_token(Token::Type::WHILE);
    } else if (current == "native") {
        return make_token(Token::Type::NATIVE);
    } else if (current == "for") {
        return make_token(Token::Type::FOR);
    } else if (current == "private") {
        return make_token(Token::Type::PRIVATE);
    } else if (current == "static") {
        return make_token(Token::Type::STATIC);
    } else if (current == "abstract") {
        return make_token(Token::Type::ABSTRACT);
    } else if (current == "override") {
        return make_token(Token::Type::OVERRDIE);
    }
    return make_token(Token::Type::IDENTIFIER);
}

std::expected<Token, Lexer::Error> Lexer::string() {
    while (!at_end() && current() != '"') {
        advance();
    }

    if (current() != '"') {
        return make_error("Expected '\"' after string literal.'");
    }

    advance(); // eat closing "
    return make_token(Token::Type::STRING);
}

Token Lexer::integer_or_number() {
    while (is_number_literal_char(current())) {
        advance();
    }
    // if no dot separator
    if (!match('.')) {
        return make_token(Token::Type::INTEGER);
    }
    // handle part after dot
    while (is_number_literal_char(current())) {
        advance();
    }
    return make_token(Token::Type::NUMBER);
}

Token Lexer::label() {
    advance(); // skip @
    consume_identifier();
    return make_token(Token::Type::LABEL);
}
