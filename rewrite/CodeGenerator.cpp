#include "CodeGenerator.h"

#include <cassert>

void CodeGenerator::visit_expr(const Expr& expr) {
    std::visit(overloaded {
        [this](const LiteralExpr& expr) { literal(expr); },
        [this](const UnaryExpr& expr) { unary(expr); },
        [this](const BinaryExpr& expr) { binary(expr); },
        [this](const StringLiteral& expr) {string_literal(expr);},
        [this](const VariableExpr& expr) {variable(expr);},
        [this](const AssigmentExpr& expr) {assigment(expr);}
    }, expr);
}

void CodeGenerator::visit_stmt(const Stmt& stmt) {
    std::visit(overloaded {
        [this](const VarStmt& expr) { var_declaration(expr); },
        [this](const ExprStmt& expr) { expr_statement(expr); },
        [this](const BlockStmt& stmt) {block_statement(stmt);},
        [this](const IfStmt& stmt) {if_statement(stmt);},
        [this](const WhileStmt& stmt) {while_statement(stmt);}
    }, stmt);
}


void CodeGenerator::unary(const UnaryExpr &expr) {
    visit_expr(*expr.expr);
    switch (expr.op) {
        case Token::Type::MINUS:
            module.write(OpCode::NEGATE);
            break;
        case Token::Type::BANG:
            module.write(OpCode::NOT);
            break;
        default:
            throw Error("Unexepected expression operator.");
    }
}

void CodeGenerator::logical(const BinaryExpr &expr) {
    int jump = start_jump(expr.op == Token::Type::AND_AND ? OpCode::JUMP_IF_FALSE : OpCode::JUMP_IF_TRUE);
    module.write(OpCode::POP);
    visit_expr(*expr.right);
    patch_jump(jump);
}

void CodeGenerator::binary(const BinaryExpr &expr) {
    visit_expr(*expr.left);
    if (expr.op == Token::Type::AND_AND || expr.op == Token::Type::BAR_BAR) {
        logical(expr);
        return;
    }

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

int CodeGenerator::start_jump(OpCode code) {
    module.write(code);
    module.write(static_cast<uint8_t>(0xFF));
    module.write(static_cast<uint8_t>(0xFF));
    return module.get_code_length() - 3; // start position of jump instruction
}

void CodeGenerator::patch_jump(int instruction_pos) {
    int offset = module.get_code_length() - instruction_pos - 3;
    // check jump
    module.patch(instruction_pos + 1, static_cast<uint8_t>(offset >> 8 & 0xFF));
    module.patch(instruction_pos + 2, static_cast<uint8_t>(offset & 0xFF));
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

void CodeGenerator::if_statement(const IfStmt &stmt) {
    visit_expr(*stmt.condition);
    int jump_to_else = start_jump(OpCode::JUMP_IF_FALSE);
    module.write(OpCode::POP);
    visit_stmt(*stmt.then_stmt);
    int jump_to_end = start_jump(OpCode::JUMP);
    patch_jump(jump_to_else);
    module.write(OpCode::POP);
    if (stmt.else_stmt) {
        visit_stmt(*stmt.else_stmt);
    }
    patch_jump(jump_to_end);
}

void CodeGenerator::begin_scope() {
    ++current_depth;
}

void CodeGenerator::end_scope() {
    --current_depth;
    while (!locals.empty() && locals.back().second > current_depth) {
        module.write(OpCode::POP);
        locals.pop_back();
    }
}

void CodeGenerator::while_statement(const WhileStmt &stmt) {
    int loop_start = module.get_code_length();
    visit_expr(*stmt.condition);
    int jump = start_jump(OpCode::JUMP_IF_FALSE);
    module.write(OpCode::POP);
    visit_stmt(*stmt.stmt);
    module.write(OpCode::LOOP);
    int offset = module.get_code_length() - loop_start + 2;
    module.write(static_cast<uint8_t>(offset >> 8 & 0xFF));
    module.write(static_cast<uint8_t>(offset & 0xFF));
    patch_jump(jump);
    module.write(OpCode::POP);
}

void CodeGenerator::block_statement(const BlockStmt &stmt) {
    begin_scope();
    for (auto& st : stmt.stmts) {
        visit_stmt(*st);
    }
    end_scope();
}

void CodeGenerator::assigment(const AssigmentExpr &expr) {
    int idx = -1;
    for (int i = locals.size() - 1; i >= 0; --i) {
        if (expr.identifier.get_lexeme(source) == locals[i].first) {
            idx = i;
        }
    }
    assert(idx != -1); // todo: error handling
    visit_expr(*expr.expr);
    module.write(OpCode::SET);
    module.write(static_cast<uint8_t>(idx));
}

void CodeGenerator::expr_statement(const ExprStmt &expr) {
    visit_expr(*expr.expr);
    module.write(OpCode::POP);
}

void CodeGenerator::variable(const VariableExpr &expr) {
    int idx = -1;
    for (int i = locals.size() - 1; i >= 0; --i) {
        if (expr.identifier.get_lexeme(source) == locals[i].first) {
            idx = i;
        }
    }
    assert(idx != -1); // todo: error handling
    module.write(OpCode::GET);
    module.write(static_cast<uint8_t>(idx)); // todo: handle overflow
}

void CodeGenerator::var_declaration(const VarStmt &expr) {
    for (int i = locals.size() - 1; i >= 0; --i) {
        auto& [name, depth] = locals[i];
        if (depth < current_depth) {
            break;
        }
        if (name == expr.name.get_lexeme(source)) {
            throw Error("Variable redeclaration is disallowed.");
        }
    }
    visit_expr(*expr.value);
    locals.emplace_back(expr.name.get_lexeme(source), current_depth);
}

void CodeGenerator::literal(const LiteralExpr& expr) {
    int index = module.add_constant(expr.literal);
    module.write(OpCode::CONSTANT);
    module.write(static_cast<uint8_t>(index)); // handle overflow!!!
}
