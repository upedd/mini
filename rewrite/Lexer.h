#ifndef PARSER_H
#define PARSER_H
#include <expected>
#include <istream>

#include "Token.h"

// maybe lexer should work on streams
// should be an easy rewrite if needed.

/**
 * Given string with code produces tokens.\n
 * Source must be valid for entire lexer lifetime.
 */
class Lexer {
public:
    struct Error {
        int source_offset = 0;
        std::string message;
    };

    explicit Lexer(const std::string_view source) : source(source) {};

    std::expected<Token, Error> next_token();
private:
    char advance();
    [[nodiscard]] char current() const;
    bool match(char c);
    [[nodiscard]] bool at_end() const;

    void skip_whitespace();
    void consume_identifier();

    [[nodiscard]] Token make_token(Token::Type type) const;
    [[nodiscard]] std::unexpected<Error> make_error(const std::string &message) const;

    Token keyword_or_identifier();
    std::expected<Token, Error> string();
    Token integer_or_number();

    int current_pos = 0;
    int start_pos = 0;

    std::string_view source;
};


#endif //PARSER_H
