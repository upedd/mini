#include "Parser.h"

#include <optional>

Token Parser::advance() {
    current = next;
    next = lexer.next_token().value(); // todo handle errors!
    return next;
}

Expr Parser::integer() const {
    // todo temp
    return LiteralExpr {std::stoi(current.get_lexeme(lexer.get_source()))};
}

Expr Parser::unary(Token::Type op) {
    return UnaryExpr { std::make_unique<Expr>(expression()), op };
}

Expr Parser::prefix() {
    switch (current.type) {
        case Token::Type::INTEGER:
            return integer();
        case Token::Type::BANG:
            return unary(current.type);
        default:
            throw Error(next, "Unexpected token.");
    }
}

Expr Parser::infix(Expr left) {
    switch (current.type) {
        case Token::Type::STAR:
        case Token::Type::PLUS:
            return binary(std::move(left));
        default:
            return left;
    }
}


Expr Parser::binary(Expr left) {
    Token::Type op = current.type;
    return BinaryExpr {std::make_unique<Expr>(std::move(left)),
        std::make_unique<Expr>(expression(get_precendece(op))), op};
}

Parser::Precedence Parser::get_precendece(Token::Type token) {
    switch (token) {
        case Token::Type::PLUS:
            return Precedence::TERM;
        case Token::Type::STAR:
            return Precedence::FACTOR;
        default:
            return Precedence::NONE;
    }
}

Expr Parser::expression(Precedence precedence) {
    advance();
    auto left = prefix();
    while (precedence < get_precendece(next.type)) {
        advance();
        left = infix(std::move(left));
    }
    return left;
}
