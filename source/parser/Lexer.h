#ifndef LEXER_H
#define LEXER_H
#include <expected>
#include <stack>
#include <vector>

#include "Token.h"
#include "../base/stream.h"

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
                                                                               stream(std::move(stream)) {
        state.emplace();
    };

    std::expected<Token, bite::Diagnostic> next_token();

    [[nodiscard]] const std::string& get_filepath() const {
        return stream.get_filepath();
    }

private:
    void skip_whitespace();
    void consume_identifier();

    [[nodiscard]] Token make_token(Token::Type type);
    [[nodiscard]] std::unexpected<bite::Diagnostic> make_error(
        const std::string& reason,
        const std::string& inline_message = ""
    );

    Token keyword_or_identifier();
    std::optional<std::unexpected<bite::Diagnostic>> consume_unicode_scalar();
    std::expected<Token, bite::Diagnostic> string();
    Token integer_or_number();
    Token label();

    /**
     * Note that our parser must contain recursive structure in order to support string interpolation.
     * The idea is when we start interpolation in a string, we push new state onto the stack, then we track brackets
     * and increase or decrase bracket depth accordingly when our bracket depth becomes negative we know that
     * the code fragment, in a string interpolation which pushed this state, ended and can go back into parsing the string.
     *
     * Example:
     * let string = "Hey ${if name != nil { name } else { "stranger" }}!"
     *                    ^               ^      ^      |+1          ^^
     *                    |               |      | -1                || -1, bracket depth is negative
     *                    |               | +1 to depth              ||     pop back state and continue parsing the string
     *                    | push new state exit string parsing       | -1
     *                    | emit Token::string_part
     */
    struct ParserState {
        int bracket_depth = 0;
    };

    std::stack<ParserState> state;

    /**
     * In string interpolation we also support putting variable name directly after '$' without brackets.
     * Instead of using the state stack defined above, we represent state using
     * these booleans values, in order to parse this construct appropriately
     */
    bool consume_identifer_on_next = false;
    bool continue_string_on_next = false;

    std::size_t start_pos = 0;
    std::string buffer;
    SharedContext* context;
    bite::file_input_stream stream;
};

#endif //LEXER_H
