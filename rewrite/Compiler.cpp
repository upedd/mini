#include "Compiler.h"

#include <cassert>

void Compiler::visit_stmt(const Stmt &statement) {
    std::visit(overloaded{
                   [this](const VarStmt &stmt) { variable_declaration(stmt); },
                   [this](const FunctionStmt &stmt) { function_declration(stmt); },
                   [this](const ExprStmt &stmt) { expr_statement(stmt); },
                   [this](const BlockStmt &stmt) { block_statement(stmt); },
                   [this](const IfStmt &stmt) { if_statement(stmt); },
                   [this](const WhileStmt &stmt) { while_statement(stmt); },
                   [this](const ReturnStmt &stmt) { return_statement(stmt); },
               }, statement);
}

void Compiler::variable_declaration(const VarStmt &expr) {
    for (int i = get_current_locals().size() - 1; i >= 0; --i) {
        auto &[name, depth] = get_current_locals()[i];
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

void Compiler::function_declration(const FunctionStmt &stmt) {
    std::string function_name = std::string(stmt.name.get_lexeme(source));
    Function *function = allocate_function();
    function->name = function_name;
    function->arity = stmt.params.size();

    int constant = current_function().add_constant(function);
    emit(OpCode::CONSTANT);
    emit(static_cast<uint8_t>(constant));
    define_variable(function_name);

    states.emplace_back(function);
    begin_scope();
    define_variable(function_name);
    for (const Token &param: stmt.params) {
        define_variable(param.get_lexeme(source));
    }
    visit_stmt(*stmt.body);
    // emit default retrun
    emit(OpCode::NIL);
    emit(OpCode::RETURN);
    end_scope();
    states.pop_back();
}

void Compiler::expr_statement(const ExprStmt &expr) {
    visit_expr(*expr.expr);
    emit(OpCode::POP);
}

void Compiler::block_statement(const BlockStmt &stmt) {
    begin_scope();
    for (auto &st: stmt.stmts) {
        visit_stmt(*st);
    }
    end_scope();
}

void Compiler::if_statement(const IfStmt &stmt) {
    visit_expr(*stmt.condition);
    int jump_to_else = start_jump(OpCode::JUMP_IF_FALSE);
    emit(OpCode::POP);
    visit_stmt(*stmt.then_stmt);
    int jump_to_end = start_jump(OpCode::JUMP);
    patch_jump(jump_to_else);
    emit(OpCode::POP);
    if (stmt.else_stmt) {
        visit_stmt(*stmt.else_stmt);
    }
    patch_jump(jump_to_end);
}

void Compiler::while_statement(const WhileStmt &stmt) {
    int loop_start = current_program().size();
    visit_expr(*stmt.condition);
    int jump = start_jump(OpCode::JUMP_IF_FALSE);
    emit(OpCode::POP);
    visit_stmt(*stmt.stmt);
    emit(OpCode::LOOP);
    int offset = current_program().size() - loop_start + 2;
    emit(static_cast<uint8_t>(offset >> 8 & 0xFF));
    emit(static_cast<uint8_t>(offset & 0xFF));
    patch_jump(jump);
    emit(OpCode::POP);
}

void Compiler::return_statement(const ReturnStmt &stmt) {
    if (stmt.expr != nullptr) {
        visit_expr(*stmt.expr);
    } else {
        emit(OpCode::NIL);
    }
    emit(OpCode::RETURN);
}

void Compiler::visit_expr(const Expr &expression) {
    std::visit(overloaded{
                   [this](const LiteralExpr &expr) { literal(expr); },
                   [this](const UnaryExpr &expr) { unary(expr); },
                   [this](const BinaryExpr &expr) { binary(expr); },
                   [this](const StringLiteral &expr) { string_literal(expr); },
                   [this](const VariableExpr &expr) { variable(expr); },
                   [this](const AssigmentExpr &expr) { assigment(expr); },
                   [this](const CallExpr &expr) { call(expr); }
               }, expression);
}

void Compiler::literal(const LiteralExpr &expr) {
    int index = current_function()->add_constant(expr.literal);
    emit(OpCode::CONSTANT);
    emit(index); // handle overflow!!!
}

void Compiler::unary(const UnaryExpr &expr) {
    visit_expr(*expr.expr);
    switch (expr.op) {
        case Token::Type::MINUS: emit(OpCode::NEGATE);
            break;
        case Token::Type::BANG: emit(OpCode::NOT);
            break;
        case Token::Type::TILDE: emit(OpCode::BINARY_NOT);
            break;
        default: throw Error("Unexepected expression operator.");
    }
}

void Compiler::binary(const BinaryExpr &expr) {
    visit_expr(*expr.left);

    if (expr.op == Token::Type::AND_AND || expr.op == Token::Type::BAR_BAR) {
        logical(expr);
        return;
    }

    visit_expr(*expr.right);
    switch (expr.op) {
        case Token::Type::PLUS: emit(OpCode::ADD);
            break;
        case Token::Type::MINUS: emit(OpCode::SUBTRACT);
            break;
        case Token::Type::STAR: emit(OpCode::MULTIPLY);
            break;
        case Token::Type::SLASH: emit(OpCode::DIVIDE);
            break;
        case Token::Type::EQUAL_EQUAL: emit(OpCode::EQUAL);
            break;
        case Token::Type::BANG_EQUAL: emit(OpCode::NOT_EQUAL);
            break;
        case Token::Type::LESS: emit(OpCode::LESS);
            break;
        case Token::Type::LESS_EQUAL: emit(OpCode::LESS_EQUAL);
            break;
        case Token::Type::GREATER: emit(OpCode::GREATER);
            break;
        case Token::Type::GREATER_EQUAL: emit(OpCode::GREATER_EQUAL);
            break;
        case Token::Type::GREATER_GREATER: emit(OpCode::RIGHT_SHIFT);
            break;
        case Token::Type::LESS_LESS: emit(OpCode::LEFT_SHIFT);
            break;
        case Token::Type::AND: emit(OpCode::BITWISE_AND);
            break;
        case Token::Type::BAR: emit(OpCode::BITWISE_OR);
            break;
        case Token::Type::CARET: emit(OpCode::BITWISE_XOR);
            break;
        case Token::Type::PERCENT: emit(OpCode::MODULO);
            break;
        case Token::Type::SLASH_SLASH: emit(OpCode::FLOOR_DIVISON);
            break;
        default: throw Error("Unexepected expression operator.");
    }
}

void Compiler::logical(const BinaryExpr &expr) {
    int jump = start_jump(expr.op == Token::Type::AND_AND ? OpCode::JUMP_IF_FALSE : OpCode::JUMP_IF_TRUE);
    emit(OpCode::POP);
    visit_expr(*expr.right);
    patch_jump(jump);
}

void Compiler::string_literal(const StringLiteral &expr) {
    int index = current_module().add_string_constant(expr.string);
    emit(OpCode::CONSTANT);
    emit(index); // handle overflow!!!
}

void Compiler::variable(const VariableExpr &expr) {
    int idx = -1;
    for (int i = get_current_locals().size() - 1; i >= 0; --i) {
        if (expr.identifier.get_lexeme(source) == get_current_locals()[i].first) {
            idx = i;
        }
    }
    assert(idx != -1); // todo: error handling
    emit(OpCode::GET);
    emit(static_cast<uint8_t>(idx)); // todo: handle overflow
}

void Compiler::assigment(const AssigmentExpr &expr) {
    int idx = -1;
    for (int i = get_current_locals().size() - 1; i >= 0; --i) {
        if (expr.identifier.get_lexeme(source) == get_current_locals()[i].first) {
            idx = i;
        }
    }
    assert(idx != -1); // todo: error handling
    visit_expr(*expr.expr);
    emit(OpCode::SET);
    emit(static_cast<uint8_t>(idx));
}


void Compiler::call(const CallExpr &expr) {
    visit_expr(*expr.callee);
    for (const ExprHandle &argument: expr.arguments) {
        visit_expr(*argument);
    }
    emit(OpCode::CALL);
    emit(static_cast<uint8_t>(expr.arguments.size()));
}

int Compiler::start_jump(OpCode code) {
    emit(code);
    emit(0xFF);
    emit(0xFF);
    return current_module().get_code_length() - 3; // start position of jump instruction
}

void Compiler::patch_jump(int instruction_pos) {
    int offset = current_module().get_code_length() - instruction_pos - 3;
    // check jump
    current_module().patch(instruction_pos + 1, static_cast<uint8_t>(offset >> 8 & 0xFF));
    current_module().patch(instruction_pos + 2, static_cast<uint8_t>(offset & 0xFF));
}

int &Compiler::get_current_depth() {
    return states.back().current_depth;
}

Function *Compiler::current_function() const {
    return states.back().function;
}

std::vector<std::pair<std::string, int> > &Compiler::get_current_locals() {
    return states.back().locals;
}

void Compiler::compile() {
    for (auto &stmt: parser.parse()) {
        visit_stmt(stmt);
    }
}

const Function &Compiler::get_main() const {
    return main;
}

Function *Compiler::get_function() {
    return current_function();
}

void Compiler::define_variable(const std::string &variable_name) {
    for (int i = get_current_locals().size() - 1; i >= 0; --i) {
        auto &[name, depth] = get_current_locals()[i];
        if (depth < get_current_depth()) {
            break;
        }
        if (name == variable_name) {
            throw Error("Function redeclaration is disallowed.");
        }
    }
    get_current_locals().emplace_back(variable_name, get_current_depth());
}





void Compiler::emit(OpCode op_code) {
    current_function()->get_program().write(op_code);
}

void Compiler::emit(bite_byte byte) {
    current_function()->get_program().write(byte);
}


void Compiler::begin_scope() {
    ++get_current_depth();
}

void Compiler::end_scope() {
    --get_current_depth();
    while (!get_current_locals().empty() && get_current_locals().back().second > get_current_depth()) {
        emit(OpCode::POP);
        get_current_locals().pop_back();
    }
}

Program &Compiler::current_program() {
    return current_function()->get_program();
}





