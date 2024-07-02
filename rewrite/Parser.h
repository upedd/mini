#ifndef PARSER_H
#define PARSER_H

#include <vector>

#include "Lexer.h"
#include "Expr.h"

#include "Stmt.h"
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
    // C like precedence
    // References:
    // https://en.wikipedia.org/wiki/Operators_in_C_and_C%2B%2B#Operator_precedence
    // https://en.cppreference.com/w/cpp/language/operator_precedence
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
        PRIMARY // literal or variable
    };

    static Precedence get_precendece(Token::Type token);

    // Parser handles errors by quietly storing them instead of stopping.
    // We want to continue parsing in order to provide user multiple issues in their code at once.
    class Error {
    public:
        Error(Token token, std::string_view message) : message(message), token(token) {}
        Token token;
        std::string message;
    };

    const std::vector<Error>& get_errors();

    explicit Parser(const Lexer &lexer) : lexer(lexer) {} // todo: not final

    bool match(Token::Type type);

    Stmt var_declaration();

    Stmt expr_statement();

    bool check(Token::Type type);

    Stmt block();

    Stmt if_statement();

    Stmt while_statement();

    Stmt function_declaration();

    Stmt return_statement();

    Stmt declaration();

    std::vector<Stmt> parse();
private:
    Expr expression(Precedence precedence = Precedence::NONE);
    void error(const Token& token, std::string_view message);
    bool panic_mode = false;
    std::vector<Error> errors;

    Token advance();
    void consume(Token::Type type, std::string_view message);

    Expr call(Expr left);

    Expr infix(Expr left);

    Expr number();
    Expr literal();

    Expr string();

    Expr identifier();

    std::optional<Expr> prefix();

    Expr grouping();
    Expr unary(Token::Type op);
    Expr binary(Expr left);

    Expr integer() const;

    Token current;
    Token next;
    Lexer lexer;
};


#endif //PARSER_H
