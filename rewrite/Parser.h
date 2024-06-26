#ifndef PARSER_H
#define PARSER_H
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
    explicit Parser(const Lexer &lexer) : lexer(lexer) {} // todo not final
    Expr expression();
private:
    Token advance();

    Token previous;
    Token current;
    Lexer lexer;
};


#endif //PARSER_H
