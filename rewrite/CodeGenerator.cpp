#include "CodeGenerator.h"

#include <cassert>

void CodeGenerator::visit_expr(const Expr& expr) {
    std::visit(overloaded {
        [this](const LiteralExpr& expr) { literal(expr); },
        [this](const UnaryExpr& expr) { unary(expr); },
        [this](const BinaryExpr& expr) { binary(expr); },
        [this](const StringLiteral& expr) {string_literal(expr);},
        [this](const VariableExpr& expr) {variable(expr);},
        [this](const AssigmentExpr& expr) {assigment(expr);},
            [this](const CallExpr& expr) {call(expr);}
    }, expr);
}

void CodeGenerator::visit_stmt(const Stmt& stmt) {
    std::visit(overloaded {
        [this](const VarStmt& expr) { var_declaration(expr); },
        [this](const ExprStmt& expr) { expr_statement(expr); },
        [this](const BlockStmt& stmt) {block_statement(stmt);},
        [this](const IfStmt& stmt) {if_statement(stmt);},
        [this](const WhileStmt& stmt) {while_statement(stmt);},
        [this](const FunctionStmt& stmt) {function_statement(stmt);}
    }, stmt);
}


void CodeGenerator::unary(const UnaryExpr &expr) {
    visit_expr(*expr.expr);
    switch (expr.op) {
        case Token::Type::MINUS:
            current_module().write(OpCode::NEGATE);
            break;
        case Token::Type::BANG:
            current_module().write(OpCode::NOT);
            break;
        case Token::Type::TILDE:
            current_module() .write(OpCode::BINARY_NOT);
            break;
        default:
            throw Error("Unexepected expression operator.");
    }
}

void CodeGenerator::logical(const BinaryExpr &expr) {
    int jump = start_jump(expr.op == Token::Type::AND_AND ? OpCode::JUMP_IF_FALSE : OpCode::JUMP_IF_TRUE);
    current_module().write(OpCode::POP);
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
            current_module().write(OpCode::ADD);
            break;
        case Token::Type::MINUS:
            current_module().write(OpCode::SUBTRACT);
            break;
        case Token::Type::STAR:
            current_module().write(OpCode::MULTIPLY);
            break;
        case Token::Type::SLASH:
            current_module().write(OpCode::DIVIDE);
            break;
        case Token::Type::EQUAL_EQUAL:
            current_module().write(OpCode::EQUAL);
            break;
        case Token::Type::BANG_EQUAL:
            current_module().write(OpCode::NOT_EQUAL);
            break;
        case Token::Type::LESS:
            current_module().write(OpCode::LESS);
            break;
        case Token::Type::LESS_EQUAL:
            current_module().write(OpCode::LESS_EQUAL);
            break;
        case Token::Type::GREATER:
            current_module().write(OpCode::GREATER);
            break;
        case Token::Type::GREATER_EQUAL:
            current_module().write(OpCode::GREATER_EQUAL);
            break;
        case Token::Type::GREATER_GREATER:
            current_module().write(OpCode::RIGHT_SHIFT);
            break;
        case Token::Type::LESS_LESS:
            current_module().write(OpCode::LEFT_SHIFT);
            break;
        case Token::Type::AND:
            current_module().write(OpCode::BITWISE_AND);
            break;
        case Token::Type::BAR:
            current_module().write(OpCode::BITWISE_OR);
            break;
        case Token::Type::CARET:
            current_module().write(OpCode::BITWISE_XOR);
            break;
        case Token::Type::PERCENT:
            current_module().write(OpCode::MODULO);
            break;
        case Token::Type::SLASH_SLASH:
            current_module().write(OpCode::FLOOR_DIVISON);
            break;
        default:
            throw Error("Unexepected expression operator.");
    }
}

void CodeGenerator::string_literal(const StringLiteral &expr) {
    int index = current_module().add_string_constant(expr.string);
    current_module().write(OpCode::CONSTANT);
    current_module().write(static_cast<uint8_t>(index)); // handle overflow!!!
}

int CodeGenerator::start_jump(OpCode code) {
    current_module().write(code);
    current_module().write(static_cast<uint8_t>(0xFF));
    current_module().write(static_cast<uint8_t>(0xFF));
    return current_module().get_code_length() - 3; // start position of jump instruction
}

void CodeGenerator::patch_jump(int instruction_pos) {
    int offset = current_module().get_code_length() - instruction_pos - 3;
    // check jump
    current_module().patch(instruction_pos + 1, static_cast<uint8_t>(offset >> 8 & 0xFF));
    current_module().patch(instruction_pos + 2, static_cast<uint8_t>(offset & 0xFF));
}

int &CodeGenerator::get_current_depth() {
    return states.back().current_depth;
}

Function* CodeGenerator::get_current_function() const {
    return states.back().function;
}

std::vector<std::pair<std::string, int>> CodeGenerator::get_current_locals() {
    return states.back().locals;
}

Module& CodeGenerator::current_module() const {
    return get_current_function()->code;
}


void CodeGenerator::generate(const std::vector<Stmt>& stmts, std::string_view source) {
    this->source = source;
    for (auto& stmt : stmts) {
        visit_stmt(stmt);
    }
}

Module CodeGenerator::get_module() {
    return current_module();
}

Function* CodeGenerator::get_function() {
    return get_current_function();
}

void CodeGenerator::define_variable(const std::string& variable_name) {
    for (int i = get_current_locals().size() - 1; i >= 0; --i) {
        auto& [name, depth] = get_current_locals()[i];
        if (depth < get_current_depth()) {
            break;
        }
        if (name == variable_name) {
            throw Error("Function redeclaration is disallowed.");
        }
    }
    get_current_locals().emplace_back(variable_name, get_current_depth());
}

void CodeGenerator::function_statement(const FunctionStmt &stmt) {
    std::string function_name = std::string(stmt.name.get_lexeme(source));
    Function* function = allocate_function();
    function->name = function_name;
    function->arity = stmt.params.size();

    int constant = current_module().add_constant(function);
    current_module().write(OpCode::CONSTANT);
    current_module().write(static_cast<uint8_t>(constant));
    define_variable(function_name);

    states.emplace_back(function);
    for (const Token& param : stmt.params) {
        define_variable(param.get_lexeme(source));
    }
    visit_stmt(*stmt.body);
    states.pop_back();
}

void CodeGenerator::call(const CallExpr &expr) {
    visit_expr(*expr.callee);
    for (const ExprHandle& argument : expr.arguments) {
        visit_expr(*argument);
    }
    current_module().write(OpCode::CALL);
    current_module().write(static_cast<uint8_t>(expr.arguments.size()));
}

void CodeGenerator::if_statement(const IfStmt &stmt) {
    visit_expr(*stmt.condition);
    int jump_to_else = start_jump(OpCode::JUMP_IF_FALSE);
    current_module().write(OpCode::POP);
    visit_stmt(*stmt.then_stmt);
    int jump_to_end = start_jump(OpCode::JUMP);
    patch_jump(jump_to_else);
    current_module().write(OpCode::POP);
    if (stmt.else_stmt) {
        visit_stmt(*stmt.else_stmt);
    }
    patch_jump(jump_to_end);
}

void CodeGenerator::begin_scope() {
    ++get_current_depth();
}

void CodeGenerator::end_scope() {
    --get_current_depth();
    while (!get_current_locals().empty() && get_current_locals().back().second > get_current_depth()) {
        current_module().write(OpCode::POP);
        get_current_locals().pop_back();
    }
}

void CodeGenerator::while_statement(const WhileStmt &stmt) {
    int loop_start = current_module().get_code_length();
    visit_expr(*stmt.condition);
    int jump = start_jump(OpCode::JUMP_IF_FALSE);
    current_module().write(OpCode::POP);
    visit_stmt(*stmt.stmt);
    current_module().write(OpCode::LOOP);
    int offset = current_module().get_code_length() - loop_start + 2;
    current_module().write(static_cast<uint8_t>(offset >> 8 & 0xFF));
    current_module().write(static_cast<uint8_t>(offset & 0xFF));
    patch_jump(jump);
    current_module().write(OpCode::POP);
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
    for (int i = get_current_locals().size() - 1; i >= 0; --i) {
        if (expr.identifier.get_lexeme(source) == get_current_locals()[i].first) {
            idx = i;
        }
    }
    assert(idx != -1); // todo: error handling
    visit_expr(*expr.expr);
    current_module().write(OpCode::SET);
    current_module().write(static_cast<uint8_t>(idx));
}

void CodeGenerator::expr_statement(const ExprStmt &expr) {
    visit_expr(*expr.expr);
    current_module().write(OpCode::POP);
}

void CodeGenerator::variable(const VariableExpr &expr) {
    int idx = -1;
    for (int i = get_current_locals().size() - 1; i >= 0; --i) {
        if (expr.identifier.get_lexeme(source) == get_current_locals()[i].first) {
            idx = i;
        }
    }
    assert(idx != -1); // todo: error handling
    current_module().write(OpCode::GET);
    current_module().write(static_cast<uint8_t>(idx)); // todo: handle overflow
}

void CodeGenerator::var_declaration(const VarStmt &expr) {
    for (int i = get_current_locals().size() - 1; i >= 0; --i) {
        auto& [name, depth] = get_current_locals()[i];
        if (depth < get_current_depth()) {
            break;
        }
        if (name == expr.name.get_lexeme(source)) {
            throw Error("Variable redeclaration is disallowed.");
        }
    }
    visit_expr(*expr.value);
    get_current_locals().emplace_back(expr.name.get_lexeme(source), get_current_depth());
}

void CodeGenerator::literal(const LiteralExpr& expr) {
    int index = current_module().add_constant(expr.literal);
    current_module().write(OpCode::CONSTANT);
    current_module().write(static_cast<uint8_t>(index)); // handle overflow!!!
}
