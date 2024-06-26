#include "Parser.h"

#include <optional>

Token Parser::advance() {
    previous = current;
    current = lexer.next_token().value(); // todo handle errors!
    return current;
}

Expr Parser::integer(const Token& token) {
    // todo temp
    return LiteralExpr {std::stoi(previous.get_lexeme(lexer.get_source()))};
}

Expr Parser::prefix(const Token &token) {
    switch (previous.type) {
        case Token::Type::INTEGER:
            return integer(previous);
        case Token::Type::BANG:
            return UnaryExpr {make_unique<Expr>(expression(0)), previous.type};
        default:
            throw Error(current, "Unexpected token.");
    }
}

// std::optional<Expr> Parser::infix(const Expr& expr, const Token &token) {
//     Expr right = expression();
//     switch (token.type) {
//         case Token::Type::PLUS:
//         case Token::Type::STAR:
//             return BinaryExpr {std::make_unique<Expr>(std::move(expr)), std::make_unique<Expr>(std::move(right)), token.type};
//     }
// }


int Parser::get_precendece() {
    switch (current.type) {
        case Token::Type::PLUS:
            return 3;
        case Token::Type::STAR:
            return 4;
        default:
            return 0;
    }
}

Expr Parser::expression(int precedence) {
    advance();
    auto left = prefix(current);
    while (precedence < get_precendece()) {
        advance();
        auto bound = previous;
        Expr right = expression(previous.type == Token::Type::PLUS ? 3 : 4);
        switch (bound.type) {
            case Token::Type::PLUS:
            case Token::Type::STAR:
                return BinaryExpr {std::make_unique<Expr>(std::move(left)), std::make_unique<Expr>(std::move(right)), bound.type};
            default:
                return left;
        }
    }
    return left;
}
