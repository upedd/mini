#ifndef PARSER_H
#define PARSER_H
#include <optional>

#include "Lexer.h"
#include "Expr.h"

/**
 * Implementation of Pratt parser
 * References:
 * https://journal.stuffwithstuff.com/2011/03/19/pratt-parsers-expression-parsing-made-easy/
 * https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html
 * https://en.wikipedia.org/w/index.php?title=Operator-precedence_parser
 */
class Parser {
public:
    class Error : public std::runtime_error {
    public:
        Error(Token token, std::string_view message) : std::runtime_error(message.data()), token(token) {}
        Token token;
    };

    explicit Parser(const Lexer &lexer) : lexer(lexer) {advance();} // todo not final
    Expr integer(const Token &token);
    Expr prefix(const Token& token);

    std::optional<Expr> infix(const Expr& expr, const Token &token);

    int get_precendece();

    Expr expression(int precedence = 0);
private:
    Token advance();

    Token previous;
    Token current;
    Lexer lexer;
};


#endif //PARSER_H
