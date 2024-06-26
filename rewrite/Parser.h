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
 * https://github.com/munificent/bantam/tree/master
 */
class Parser {
public:
    enum class Precedence {
        NONE,
        ASSIGMENT,
        LOGICAL_OR,
        LOGICAL_AND,
        BITWISE_OR,
        BITWISE_AND,
        BITWISE_XOR,
        EQUALITY,
        RELATIONAL,
        BITWISE_SHIFT,
        TERM,
        FACTOR,
        UNARY,
        CALL,
        PRIMARY // literal of variable
    };


    class Error : public std::runtime_error {
    public:
        Error(Token token, std::string_view message) : std::runtime_error(message.data()), token(token) {}
        Token token;
    };

    explicit Parser(const Lexer &lexer) : lexer(lexer) { // todo: not final
        advance();
    }
    Expr integer() const;

    Expr unary(Token::Type op);

    Expr prefix();

    Expr infix(Expr left);

    Expr binary(Expr left);
    Expr expression(Precedence precedence = Precedence::NONE);
private:

    static Precedence get_precendece(Token::Type token);



    Token advance();

    Token current;
    Token next;
    Lexer lexer;
};


#endif //PARSER_H
