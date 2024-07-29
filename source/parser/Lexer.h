#ifndef LEXER_H
#define LEXER_H
#include <expected>
#include <vector>

#include "../Token.h"
#include "InputStream.h"

class SharedContext;

/**
 * Given input stream with code produces tokens.
 */
class Lexer {
public:
    struct Error {
        int source_offset = 0;
        std::string message;
    };

    explicit Lexer(FileInputStream&& stream, SharedContext* context) : context(context),
                                                                       stream(std::move(stream)) {};

    std::expected<Token, Error> next_token();

private:
    void skip_whitespace();
    void consume_identifier();

    [[nodiscard]] Token make_token(Token::Type type);
    [[nodiscard]] std::unexpected<Error> make_error(const std::string& reason, const std::string& inline_message = "") const;

    Token keyword_or_identifier();
    std::expected<Token, Error> string();
    Token integer_or_number();
    Token label();

    std::size_t start_pos {};
    std::string buffer;
    SharedContext* context;
    FileInputStream stream;
};


#endif //LEXER_H
