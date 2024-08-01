#include "Lexer.h"

#include <expected>

#include "../common.h"
#include "../base/perfect_map.h"
#include "../shared/SharedContext.h"

std::expected<Token, Lexer::Error> Lexer::next_token() {
    skip_whitespace();
    start_pos = stream.position();
    switch (const char c = stream.advance(); c) {
        case '\0': return make_token(Token::Type::END);
        case '{': return make_token(Token::Type::LEFT_BRACE);
        case '}': return make_token(Token::Type::RIGHT_BRACE);
        case '(': return make_token(Token::Type::LEFT_PAREN);
        case ')': return make_token(Token::Type::RIGHT_PAREN);
        case '[': return make_token(Token::Type::LEFT_BRACKET);
        case ']': return make_token(Token::Type::RIGHT_BRACKET);
        case ',': return make_token(Token::Type::COMMA);
        case ';': return make_token(Token::Type::SEMICOLON);
        case '~': return make_token(Token::Type::TILDE);
        case ':': return make_token(Token::Type::COLON);
        case '!': return make_token(stream.match('=') ? Token::Type::BANG_EQUAL : Token::Type::BANG);
        case '+': return make_token(stream.match('=') ? Token::Type::PLUS_EQUAL : Token::Type::PLUS);
        case '-': return make_token(stream.match('=') ? Token::Type::MINUS_EQUAL : Token::Type::MINUS);
        case '*': return make_token(stream.match('=') ? Token::Type::STAR_EQUAL : Token::Type::STAR);
        case '%': return make_token(stream.match('=') ? Token::Type::PERCENT_EQUAL : Token::Type::PERCENT);
        case '^': return make_token(stream.match('=') ? Token::Type::CARET_EQUAL : Token::Type::CARET);
        case '=': return make_token(stream.match('=') ? Token::Type::EQUAL_EQUAL : Token::Type::EQUAL);
        case '&': {
            if (stream.match('&')) {
                return make_token(Token::Type::AND_AND);
            }
            return make_token(stream.match('=') ? Token::Type::AND_EQUAL : Token::Type::AND);
        }
        case '|': {
            if (stream.match('|')) {
                return make_token(Token::Type::BAR_BAR);
            }
            return make_token(stream.match('=') ? Token::Type::BAR_EQUAL : Token::Type::BAR);
        }
        case '/': {
            if (stream.match('/')) {
                return make_token(stream.match('=') ? Token::Type::SLASH_SLASH_EQUAL : Token::Type::SLASH_SLASH);
            }
            return make_token(stream.match('=') ? Token::Type::SLASH_EQUAL : Token::Type::SLASH);
        }
        case '.': {
            if (stream.match('.')) {
                return make_token(stream.match('.') ? Token::Type::DOT_DOT_DOT : Token::Type::DOT_DOT);
            }
            return make_token(Token::Type::DOT);
        }
        case '<': {
            if (stream.match('<')) {
                return make_token(stream.match('=') ? Token::Type::LESS_LESS_EQUAL : Token::Type::LESS_LESS);
            }
            return make_token(stream.match('=') ? Token::Type::LESS_EQUAL : Token::Type::LESS);
        }
        case '>': {
            if (stream.match('>')) {
                return make_token(
                    stream.match('=') ? Token::Type::GREATER_GREATER_EQUAL : Token::Type::GREATER_GREATER
                );
            }
            return make_token(stream.match('=') ? Token::Type::GREATER_EQUAL : Token::Type::GREATER);
        }
        case '"': return string();
        case '@': return label();
        default: {
            buffer.push_back(c);
            if (is_digit(c)) {
                // TODO: number literals should be able to start with dot
                return integer_or_number();
            }
            if (is_identifier(c)) {
                return keyword_or_identifier();
            }
            return make_error("invalid character", "here");
        }
    }
}


void Lexer::skip_whitespace() {
    while (true) {
        if (is_space(stream.next()) || stream.next() == '\n') {
            stream.advance();
        } else if (stream.next() == '#') {
            while (!stream.ended() && !stream.match('\n'))
                stream.advance();
        } else {
            break;
        }
    }
}

void Lexer::consume_identifier() {
    while (is_identifier(stream.next())) {
        buffer.push_back(stream.advance());
    }
}

Token Lexer::make_token(const Token::Type type) {
    const StringTable::Handle string = buffer.empty() ? nullptr : context->intern(buffer);
    buffer.clear();
    return { .type = type, .source_start_offset = start_pos, .source_end_offset = stream.position(), .string = string };
}

std::unexpected<Lexer::Error> Lexer::make_error(const std::string& reason, const std::string& inline_message) const {
    context->add_compilation_message(
        {
            .level = bite::Logger::Level::error,
            .content = reason,
            .inline_msg = bite::InlineMessage {
                .location = bite::SourceLocation {
                    .file_path = "debug.bite",
                    .start_offset = start_pos,
                    .end_offset = stream.position()
                },
                .content = inline_message
            }
        }
    );
    return std::unexpected<Error>({ static_cast<int>(start_pos), reason });
}

constexpr static perfect_map<Token::Type, 30> identifiers(
    {
        {
            { "class", Token::Type::CLASS },
            { "fun", Token::Type::FUN },
            { "return", Token::Type::RETURN },
            { "if", Token::Type::IF },
            { "is", Token::Type::IS },
            { "in", Token::Type::IN },
            { "break", Token::Type::BREAK },
            { "continue", Token::Type::CONTINUE },
            { "match", Token::Type::MATCH },
            { "true", Token::Type::TRUE },
            { "false", Token::Type::FALSE },
            { "else", Token::Type::ELSE },
            { "this", Token::Type::THIS },
            { "loop", Token::Type::LOOP },
            { "super", Token::Type::SUPER },
            { "nil", Token::Type::NIL },
            { "let", Token::Type::LET },
            { "while", Token::Type::WHILE },
            { "native", Token::Type::NATIVE },
            { "for", Token::Type::FOR },
            { "private", Token::Type::PRIVATE },
            { "abstract", Token::Type::ABSTRACT },
            { "override", Token::Type::OVERRDIE },
            { "get", Token::Type::GET },
            { "set", Token::Type::SET },
            { "object", Token::Type::OBJECT },
            { "trait", Token::Type::TRAIT },
            { "exclude", Token::Type::EXCLUDE },
            { "as", Token::Type::AS },
            { "using", Token::Type::USING }
        }
    }
);

Token Lexer::keyword_or_identifier() {
    consume_identifier();

    if (const std::optional<Token::Type> type = identifiers[buffer]) {
        return make_token(*type);
    }
    return make_token(Token::Type::IDENTIFIER);
}

std::expected<Token, Lexer::Error> Lexer::string() {
    while (!stream.ended() && stream.next() != '"') {
        buffer.push_back(stream.advance());
    }

    if (!stream.match('"')) {
        return make_error("unterminated string", "expected \" after this");
    }

    return make_token(Token::Type::STRING);
}

Token Lexer::integer_or_number() {
    while (is_number_literal_char(stream.next())) {
        buffer.push_back(stream.advance());
    }
    // if no dot exists separator it is a integer
    if (!stream.match('.')) {
        return make_token(Token::Type::INTEGER);
    }
    // otherwise it is a fraction
    while (is_number_literal_char(stream.next())) {
        buffer.push_back(stream.advance());
    }
    return make_token(Token::Type::NUMBER);
}

Token Lexer::label() {
    buffer.push_back(stream.current()); // get @
    consume_identifier();
    return make_token(Token::Type::LABEL);
}
