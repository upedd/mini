#include "Lexer.h"

#include "common.h"

Token::Type Lexer::keyword_or_identifier(char c) {
    // maybe optimize like v8 https://github.com/v8/v8/blob/e77eebfe3b747fb315bd3baad09bec0953e53e68/src/parsing/scanner.cc#L1643
    // bench if worth additional complexity

    int start = source_position - 1; // we already consumed first character
    consume_identifier();
    int size = source_position - start;
    std::string_view current = source.substr(start, size);

    if (current == "class") {
        return Token::Type::CLASS;
    } else if (current == "fun") {
        return Token::Type::FUN;
    } else if (current == "return") {
        return Token::Type::RETURN;
    } else if (current == "if") {
        return Token::Type::IF;
    } else if (current == "is") {
        return Token::Type::IS;
    } else if (current == "in") {
        return Token::Type::IN;
    } else if (current == "break") {
        return Token::Type::BREAK;
    } else if (current == "continue") {
        return Token::Type::CONTINUE;
    } else if (current == "match") {
        return Token::Type::MATCH;
    } else if (current == "true") {
        return Token::Type::TRUE;
    } else if (current == "false") {
        return Token::Type::FALSE;
    } else if (current == "else") {
        return Token::Type::ELSE;
    } else if (current == "this") {
        return Token::Type::THIS;
    } else if (current == "loop") {
        return Token::Type::LOOP;
    } else if (current == "super") {
        return Token::Type::SUPER;
    }
    return Token::Type::IDENTIFIER;
}

Token Lexer::next_token() {
    skip_whitespace();

    switch (char c = next(); c) {
        case '!':
            return Token(match('=') ? Token::Type::BANG_EQUAL : Token::Type::EQUAL);
        case '+':
            return Token(match('=') ? Token::Type::PLUS_EQUAL: Token::Type::PLUS);
        case '-':
            return Token(match('=') ? Token::Type::MINUS_EQUAL : Token::Type::MINUS);
        case '*':
            return Token(match('=') ? Token::Type::STAR_EQUAL : Token::Type::STAR);
        case '%':
            return Token(match('=') ? Token::Type::PERCENT_EQUAL : Token::Type::PERCENT);
        case '/': {
            if (match('/')) {
                return Token(match('=') ? Token::Type::SLASH_SLASH_EQUAL : Token::Type::SLASH_SLASH);
            }
            return Token(match('=') ? Token::Type::SLASH_EQUAL : Token::Type::SLASH);
        }
        case '&': {
            if (match('&')) {
                return Token(Token::Type::AND_AND);
            }
            return Token(match('=') ? Token::Type::AND_EQUAL : Token::Type::AND);
        }
        case '|': {
            if (match('|')) {
                return Token(Token::Type::BAR_BAR);
            }
            return Token(match('=') ? Token::Type::BAR_EQUAL : Token::Type::BAR);
        }
        case '^':
            return Token(match('=') ? Token::Type::CARET_EQUAL : Token::Type::CARET);
        case '=':
            return Token(match('=') ? Token::Type::EQUAL_EQUAL : Token::Type::EQUAL);
        case ':':
            next(); // todo handle if not = after symbol
            return Token(Token::Type::COLON_EQUAL);
        case '{':
            return Token(Token::Type::RIGHT_BRACE);
        case '}':
            return Token(Token::Type::LEFT_BRACE);
        case '(':
            return Token(Token::Type::LEFT_PAREN);
        case ')':
            return Token(Token::Type::RIGHT_PAREN);
        case '[':
            return Token(Token::Type::LEFT_BRACKET);
        case ']':
            return Token(Token::Type::RIGHT_BRACKET);
        case ',':
            return Token(Token::Type::COMMA);
        case ';':
            return Token(Token::Type::SEMICOLON);
        case '.': {
            if (match('.')) {
                return Token(match('.') ? Token::Type::DOT_DOT_DOT : Token::Type::DOT_DOT);
            }
            return Token(Token::Type::DOT);
        }
        case '<': {
            if (match('<')) {
                return Token(match('=') ? Token::Type::LESS_LESS_EQUAL : Token::Type::LESS_LESS);
            }
            return Token(match('=') ? Token::Type::LESS_EQUAL : Token::Type::LESS);
        }
        case '>': {
            if (match('>')) {
                return Token(match('=') ? Token::Type::GREATER_GREATER_EQUAL : Token::Type::GREATER_GREATER);
            }
            return Token(match('=') ? Token::Type::GREATER_EQUAL : Token::Type::GREATER);
        }
        case '"':
            // todo string
            return Token(Token::Type::STRING);
        case '\0':
            return Token(Token::Type::END);
        default: {
            if (is_digit(c)) {
                return Token(Token::Type::NUMBER);
            }
            return Token(keyword_or_identifier(c));
        }
    }
}

char Lexer::next() {
    if (source_position + 1 >= source.size()) return '\0';
    line_offset++;
    return source[source_position++];
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
    if (peek() == c) {
        next();
        return true;
    }
    return false;
}

void Lexer::skip_whitespace() {
    // todo comments
    while (is_space(peek())) {
        if (next() == '\n') {
            line++;
            line_offset = 0;
        }
    }
}

inline bool Lexer::is_identifier_character(const char c) {
    return is_alphanum(c) || c == '_';
}

void Lexer::consume_identifier() {
    while (is_identifier_character(peek())) {
        next();
    }
}

bool Lexer::end() {
    return source_position >= source.size();
}
