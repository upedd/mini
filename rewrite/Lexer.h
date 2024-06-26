#ifndef PARSER_H
#define PARSER_H
#include <expected>
#include <istream>

#include "Token.h"

// maybe lexer should work on streams?

/**
 * Takes an istream and produces tokens.\n
 * Source must be kept alive for whole Lexer exectution!
 */
class Lexer {
public:
    struct Error {
        int source_offset = 0;
        std::string message;
    };

    explicit Lexer(const std::string_view source) : source(source) {};

    Token keyword_or_identifier();

    std::expected<Token, Error> string();

    Token integer_or_number();

    std::expected<Token, Error> next_token();

    [[nodiscard]] std::unexpected<Error> make_error(const std::string &message) const;

    bool end();

private:
    // source traversal functions
    char advance();

    char current() const;

    char peek() const;
    char peek_next();
    bool match(char c);
    void skip_whitespace();

    Token make_token(Token::Type type) const;

    inline static bool is_identifier_character(char c);
    void consume_identifier();
    int source_position = 0;
    int start = 0;
    int line = 1;
    int line_offset = 0;

    std::string_view source;
};


#endif //PARSER_H
