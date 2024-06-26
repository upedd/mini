#include "Parser.h"

Token Parser::advance() {
    previous = current;
    current = lexer.next_token().value(); // todo handle errors!
    return current;
}

Expr Parser::expression() {
    Token token = advance();

    if (token.type == Token::Type::INTEGER) {
        return LiteralExpr {std::stoi(token.get_lexeme(lexer.get_source())) }; // todo temp
    } else if (token.type == Token::Type::BANG) {
        return UnaryExpr {make_unique<Expr>(expression()), token.type};
    }
    return LiteralExpr {0};
}
