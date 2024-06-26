#include "CodeGenerator.h"

void CodeGenerator::generate(const Expr& expr) {
    std::visit(overloaded {
        [this](const LiteralExpr& expr) { literal(expr); },
        [this](const UnaryExpr& expr) { unary(expr); },
        [this](const BinaryExpr& expr) { binary(expr); }
    }, expr);
}

void CodeGenerator::unary(const UnaryExpr &expr) {
    generate(*expr.expr);
    switch (expr.op) {
        case Token::Type::MINUS:
            module.write(OpCode::NEGATE);
            break;
        default:
            throw Error("Unexepected expression operator.");
    }
}

void CodeGenerator::binary(const BinaryExpr &expr) {
    generate(*expr.left);
    generate(*expr.right);

    switch (expr.op) {
        case Token::Type::PLUS:
            module.write(OpCode::ADD);
            break;
        case Token::Type::MINUS:
            module.write(OpCode::NEGATE);
            break;
        case Token::Type::STAR:
            module.write(OpCode::MULTIPLY);
            break;
        case Token::Type::SLASH:
            module.write(OpCode::DIVIDE);
            break;
        default:
            throw Error("Unexepected expression operator.");
    }
}

Module CodeGenerator::get_module() {
    return module;
}

void CodeGenerator::literal(const LiteralExpr& expr) {
    module.write(OpCode::PUSH_INT);
    module.write(expr.literal);
}
