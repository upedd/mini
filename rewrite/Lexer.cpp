#include "Lexer.h"

#include <expected>

#include "common.h"

Token Lexer::keyword_or_identifier() {
    // maybe optimize like v8 https://github.com/v8/v8/blob/e77eebfe3b747fb315bd3baad09bec0953e53e68/src/parsing/scanner.cc#L1643
    // bench if worth additional complexity
    consume_identifier();
    int size = source_position - start;
    std::string_view current = source.substr(start, size);

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
    }
    return make_token(Token::Type::IDENTIFIER);
}

std::expected<Token, Lexer::Error> Lexer::string() {
    while (!end() && current() != '"') {
        advance();
    }

    if (current() != '"') {
        return make_error("Expected '\"' after string literal.'");
    }

    advance(); // eat closing "
    return make_token(Token::Type::STRING);
}

Token Lexer::integer_or_number() {
    // TODO: handle more number literals features i.e. hex prefix, binary prefix, separators...
    while (is_digit(current())) {
        advance();
    }
    // if no dot separator
    if (!match('.')) {
        return make_token(Token::Type::INTEGER);
    }
    // handle part after dot
    while (is_digit(current())) {
        advance();
    }
    return make_token(Token::Type::NUMBER);
}

std::expected<Token, Lexer::Error> Lexer::next_token() {
    skip_whitespace();
    start = source_position;
    switch (char c = advance(); c) {
        case '!':
            return match('=') ? make_token(Token::Type::BANG_EQUAL) : make_token(Token::Type::EQUAL);
        case '+':
            return match('=') ? make_token(Token::Type::PLUS_EQUAL): make_token(Token::Type::PLUS);
        case '-':
            return match('=') ? make_token(Token::Type::MINUS_EQUAL) : make_token(Token::Type::MINUS);
        case '*':
            return match('=') ? make_token(Token::Type::STAR_EQUAL) : make_token(Token::Type::STAR);
        case '%':
            return match('=') ? make_token(Token::Type::PERCENT_EQUAL) : make_token(Token::Type::PERCENT);
        case '/': {
            if (match('/')) {
                return match('=') ? make_token(Token::Type::SLASH_SLASH_EQUAL) : make_token(Token::Type::SLASH_SLASH);
            }
            return match('=') ? make_token(Token::Type::SLASH_EQUAL) : make_token(Token::Type::SLASH);
        }
        case '&': {
            if (match('&')) {
                return make_token(Token::Type::AND_AND);
            }
            return match('=') ? make_token(Token::Type::AND_EQUAL) : make_token(Token::Type::AND);
        }
        case '|': {
            if (match('|')) {
                return make_token(Token::Type::BAR_BAR);
            }
            return match('=') ? make_token(Token::Type::BAR_EQUAL) : make_token(Token::Type::BAR);
        }
        case '^':
            return match('=') ? make_token(Token::Type::CARET_EQUAL) : make_token(Token::Type::CARET);
        case '=':
            return match('=') ? make_token(Token::Type::EQUAL_EQUAL) : make_token(Token::Type::EQUAL);
        case ':':
            advance(); // todo handle if not = after symbol
            return make_token(Token::Type::COLON_EQUAL);
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
        case '.': {
            if (match('.')) {
                return match('.') ? make_token(Token::Type::DOT_DOT_DOT) : make_token(Token::Type::DOT_DOT);
            }
            return make_token(Token::Type::DOT);
        }
        case '<': {
            if (match('<')) {
                return match('=') ? make_token(Token::Type::LESS_LESS_EQUAL) : make_token(Token::Type::LESS_LESS);
            }
            return match('=') ? make_token(Token::Type::LESS_EQUAL) : make_token(Token::Type::LESS);
        }
        case '>': {
            if (match('>')) {
                return match('=') ? make_token(Token::Type::GREATER_GREATER_EQUAL) : make_token(Token::Type::GREATER_GREATER);
            }
            return match('=') ? make_token(Token::Type::GREATER_EQUAL) : make_token(Token::Type::GREATER);
        }
        case '"':
            return string();
        case '\0':
            return make_token(Token::Type::END);
        default: {
            if (is_digit(c)) {
                return integer_or_number();
            }
            return keyword_or_identifier();
        }
    }
}

std::unexpected<Lexer::Error> Lexer::make_error(const std::string& message) const {
    return std::unexpected<Error>({start, message});
}

char Lexer::advance() {
    if (source_position >= source.size()) return '\0';
    line_offset++;
    return source[source_position++];
}

char Lexer::current() const {
    if (source_position >= source.size()) return '\0';
    return source[source_position];
}

char Lexer::peek() const {
    if (source_position + 1 >= source.size()) return '\0';
    return source[source_position + 1];
}

char Lexer::peek_next() {
    if (source_position + 2 >= source.size()) return '\0';
    return source[source_position + 2];
}

bool Lexer::match(char c) {
    if (current() == c) {
        advance();
        return true;
    }
    return false;
}

void Lexer::skip_whitespace() {
    // todo comments
    while (is_space(current())) {
        if (advance() == '\n') {
            advance(); // skip ';'
            line++;
            line_offset = 0;
        };
    }
}

Token Lexer::make_token(Token::Type type) const {
    return {.type = type, .source_offset = start, .length = source_position - start};
}

inline bool Lexer::is_identifier_character(const char c) {
    return is_alphanum(c) || c == '_';
}

void Lexer::consume_identifier() {
    while (is_identifier_character(current())) {
        advance();
    }
}

bool Lexer::end() {
    return source_position >= source.size();
}
