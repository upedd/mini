#ifndef LEXER_H
#define LEXER_H
#include <expected>
#include <istream>
#include <vector>

#include "Token.h"
#include "shared/SharedContext.h"

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

    explicit Lexer(const std::string_view source, SharedContext* context) : source(source), context(context) {};



    std::expected<Token, Error> next_token();

    [[nodiscard]] std::string_view get_source() const;
private:
    char advance();
    [[nodiscard]] char current() const;
    bool match(char c);
    [[nodiscard]] bool at_end() const;

    void skip_whitespace();
    void consume_identifier();

    [[nodiscard]] Token make_token(Token::Type type);
    [[nodiscard]] std::unexpected<Error> make_error(const std::string &message) const;

    Token keyword_or_identifier();
    std::expected<Token, Error> string();
    Token integer_or_number();
    Token label();

    int current_pos = 0;
    int start_pos = 0;

    std::string_view source;
    // buffer for literals
    std::string buffer;
    SharedContext* context;
};


#endif //LEXER_H
