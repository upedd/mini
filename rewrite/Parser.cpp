#include "Parser.h"

Token Parser::advance() {
    current = next;
    while (true) { // todo is this loop safe?
        auto token = lexer.next_token();
        if (!token) {
            error(current, token.error().message);
        } else {
            next = *token;
            break;
        }
    }
    return next;
}

void Parser::consume(Token::Type type, std::string_view message) {
    if (next.type == type) {
        advance();
        return;
    }
    error(current, message);
}

Expr Parser::grouping() {
    auto expr = expression();
    consume(Token::Type::RIGHT_PAREN, "Expected ')' after expression.");
    return expr;
}

Expr Parser::integer() const {
    // todo temp
    return LiteralExpr {std::stoll(current.get_lexeme(lexer.get_source()))};
}

Expr Parser::unary(Token::Type op) {
    return UnaryExpr { std::make_unique<Expr>(expression(Precedence::UNARY)), op };
}

std::optional<Expr> Parser::prefix() {
    switch (current.type) {
        case Token::Type::INTEGER:
            return integer();
        case Token::Type::BANG:
        case Token::Type::MINUS:
            return unary(current.type);
        case Token::Type::LEFT_PAREN:
            return grouping();
        default: return {};
    }
}

Expr Parser::infix(Expr left) {
    switch (current.type) {
        case Token::Type::STAR:
        case Token::Type::PLUS:
        case Token::Type::MINUS:
        case Token::Type::SLASH:
        case Token::Type::SLASH_SLASH:
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
        case Token::Type::MINUS:
            return Precedence::TERM;
        case Token::Type::STAR:
        case Token::Type::SLASH:
        case Token::Type::SLASH_SLASH:
            return Precedence::FACTOR;
        default:
            return Precedence::NONE;
    }
}

Expr Parser::expression(Precedence precedence) {
    advance();
    auto left = prefix();
    if (!left) {
        error(current, "Expected expression.");
        return {};
    }
    while (precedence < get_precendece(next.type)) {
        advance();
        left = infix(std::move(*left));
    }
    return std::move(*left);
}

void Parser::error(const Token &token, std::string_view message) {
    if (panic_mode) return;
    panic_mode = true;
    errors.emplace_back(token, message);
}

const std::vector<Parser::Error> & Parser::get_errors() {
    return errors;
}



