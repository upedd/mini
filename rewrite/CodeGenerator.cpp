#include "CodeGenerator.h"

void CodeGenerator::generate(const Expr& expr) {
    std::visit(overloaded {
        [this](const LiteralExpr& expr) { literal(expr); },
        [this](const UnaryExpr& expr) { unary(expr); },
        [this](const BinaryExpr& expr) { binary(expr); },
        [this](const StringLiteral& expr) {string_literal(expr);}
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
            module.write(OpCode::SUBTRACT);
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

void CodeGenerator::string_literal(const StringLiteral &expr) {
    int index = module.add_string_constant(expr.string);
    module.write(OpCode::CONSTANT);
    module.write(static_cast<uint8_t>(index)); // handle overflow!!!
}

Module CodeGenerator::get_module() {
    return module;
}

void CodeGenerator::literal(const LiteralExpr& expr) {
    int index = module.add_constant(expr.literal);
    module.write(OpCode::CONSTANT);
    module.write(static_cast<uint8_t>(index)); // handle overflow!!!
}
