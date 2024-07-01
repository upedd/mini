#include "CodeGenerator.h"

#include <cassert>

void CodeGenerator::visit_expr(const Expr& expr) {
    std::visit(overloaded {
        [this](const LiteralExpr& expr) { literal(expr); },
        [this](const UnaryExpr& expr) { unary(expr); },
        [this](const BinaryExpr& expr) { binary(expr); },
        [this](const StringLiteral& expr) {string_literal(expr);},
        [this](const VariableExpr& expr) {variable(expr);}
    }, expr);
}

void CodeGenerator::visit_stmt(const Stmt& stmt) {
    std::visit(overloaded {
        [this](const VarStmt& expr) { var_declaration(expr); },
        [this](const ExprStmt& expr) { expr_statement(expr); },
    }, stmt);
}


void CodeGenerator::unary(const UnaryExpr &expr) {
    visit_expr(*expr.expr);
    switch (expr.op) {
        case Token::Type::MINUS:
            module.write(OpCode::NEGATE);
            break;
        default:
            throw Error("Unexepected expression operator.");
    }
}

void CodeGenerator::binary(const BinaryExpr &expr) {
    visit_expr(*expr.left);
    visit_expr(*expr.right);

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
        case Token::Type::EQUAL_EQUAL:
            module.write(OpCode::EQUAL);
            break;
        case Token::Type::BANG_EQUAL:
            module.write(OpCode::NOT_EQUAL);
            break;
        case Token::Type::LESS:
            module.write(OpCode::LESS);
            break;
        case Token::Type::LESS_EQUAL:
            module.write(OpCode::LESS_EQUAL);
            break;
        case Token::Type::GREATER:
            module.write(OpCode::GREATER);
            break;
        case Token::Type::GREATER_EQUAL:
            module.write(OpCode::GREATER_EQUAL);
            break;
        case Token::Type::GREATER_GREATER:
            module.write(OpCode::RIGHT_SHIFT);
            break;
        case Token::Type::LESS_LESS:
            module.write(OpCode::LEFT_SHIFT);
            break;
        case Token::Type::AND:
            module.write(OpCode::BITWISE_AND);
            break;
        case Token::Type::BAR:
            module.write(OpCode::BITWISE_OR);
            break;
        case Token::Type::CARET:
            module.write(OpCode::BITWISE_XOR);
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

void CodeGenerator::generate(const std::vector<Stmt>& stmts, std::string_view source) {
    this->source = source;
    for (auto& stmt : stmts) {
        visit_stmt(stmt);
    }
}

Module CodeGenerator::get_module() {
    return module;
}

void CodeGenerator::expr_statement(const ExprStmt &expr) {
    visit_expr(*expr.expr);
    module.write(OpCode::POP);
}

void CodeGenerator::variable(const VariableExpr &expr) {
    int idx = -1;
    for (int i = locals.size() - 1; i >= 0; --i) {
        if (expr.identifier.get_lexeme(source) == locals[i]) {
            idx = i;
        }
    }
    assert(idx != -1); // todo: error handling
    module.write(OpCode::GET);
    module.write(static_cast<uint8_t>(idx)); // todo: handle overflow
}

void CodeGenerator::var_declaration(const VarStmt &expr) {
    visit_expr(*expr.value);
    locals.push_back(expr.name.get_lexeme(source));
}

void CodeGenerator::literal(const LiteralExpr& expr) {
    int index = module.add_constant(expr.literal);
    module.write(OpCode::CONSTANT);
    module.write(static_cast<uint8_t>(index)); // handle overflow!!!
}
