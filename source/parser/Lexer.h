#ifndef LEXER_H
#define LEXER_H
#include <expected>
#include <vector>

#include "Token.h"
#include "../Ast.h"
#include "../base/stream.h"
#include "../shared/Message.h"

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

    explicit Lexer(bite::file_input_stream&& stream, SharedContext* context) : context(context),
                                                                       stream(std::move(stream)) {};

    std::expected<Token, bite::Message> next_token();

    [[nodiscard]] const std::string& get_filepath() const {
        return stream.get_filepath();
    }
private:
    void skip_whitespace();
    void consume_identifier();

    [[nodiscard]] Token make_token(Token::Type type);
    [[nodiscard]] std::unexpected<bite::Message> make_error(
        const std::string& reason,
        const std::string& inline_message = ""
    ) const;

    Token keyword_or_identifier();
    std::expected<Token, bite::Message> string();
    Token integer_or_number();
    Token label();

    std::size_t start_pos = 0;
    std::string buffer;
    SharedContext* context;
    bite::file_input_stream stream;
};

#endif //LEXER_H
