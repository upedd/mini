#include "Lexer.h"

#include "common.h"

Token Lexer::next_token() {
    skip_whitespace();

    switch (char c = next(); c) {}
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
    while (is_space(peek())) {
        if (next() == '\n') {
            line++;
            line_offset = 0;
        }
    }
}

bool Lexer::end() {
    return source_position >= source.size();
}
