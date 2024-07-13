#ifndef PARSER_H
#define PARSER_H

#include <vector>

#include "Lexer.h"
#include "Ast.h"

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
    // Parser handles errors by quietly storing them instead of stopping.
    // We want to continue parsing in order to provide user multiple issues in their code at once.
    class Error {
    public:
        Error(Token token, std::string_view message) : message(message), token(token) {}
        Token token;
        std::string message;
    };

    const std::vector<Error>& get_errors();

    explicit Parser(const std::string_view source) : lexer(source) {}

    std::vector<Stmt> parse();
private:
    bool panic_mode = false;
    std::vector<Error> errors;

    void error(const Token& token, std::string_view message);
    void synchronize();

    bool match(Token::Type type);
    [[nodiscard]] bool check(Token::Type type) const;
    Token advance();
    void consume(Token::Type type, std::string_view message);



    Stmt declaration();

    Stmt native_declaration();

    std::optional<Stmt> statement();

    Stmt var_declaration();

    FunctionStmt function_declaration();
    Stmt class_declaration();

    Stmt expr_statement();
    Stmt if_statement();
    Stmt while_statement();
    Stmt return_statement();

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

    Expr expression(Precedence precedence = Precedence::NONE);

    Expr this_();

    Expr super_();

    Expr block();

    std::optional<Expr> prefix();

    Expr integer();
    Expr number();
    Expr keyword() const;
    Expr string() const;
    Expr identifier();
    Expr grouping();
    Expr unary(Token::Type op);

    Expr dot(Expr left);

    Expr infix(Expr left);

    Expr binary(Expr left, bool expect_lvalue = false);
    Expr call(Expr left);

    Token current;
    Token next;
    Lexer lexer;
};


#endif //PARSER_H
