#include "Compiler.h"

#include <cassert>

#include "debug.h"

void Compiler::visit_stmt(const Stmt &statement) {
    std::visit(overloaded{
                   [this](const VarStmt &stmt) { variable_declaration(stmt); },
                   [this](const FunctionStmt &stmt) { function_declaration(stmt); },
                   [this](const ExprStmt &stmt) { expr_statement(stmt); },
                   [this](const BlockStmt &stmt) { block_statement(stmt); },
                   [this](const IfStmt &stmt) { if_statement(stmt); },
                   [this](const WhileStmt &stmt) { while_statement(stmt); },
                   [this](const ReturnStmt &stmt) { return_statement(stmt); },
                   [this](const ClassStmt& stmt) {class_declaration(stmt);}
               }, statement);
}

void Compiler::variable_declaration(const VarStmt &expr) {
    auto name = expr.name.get_lexeme(source);
    visit_expr(*expr.value);
    if (!current_locals().define(name, get_current_depth())) {
        throw Error("Variable redefinition in same scope is disallowed."); // todo: better error handling!
    }
}

void Compiler::function_declaration(const FunctionStmt &stmt) {
    std::string function_name = std::string(stmt.name.get_lexeme(source));
    Function *function = new Function(function_name, stmt.params.size()); // TODO: memory leak!
    allocated_objects.push_back(function);
    if (!current_locals().define(function_name, get_current_depth())) {
        throw Error("Variable redefinition in same scope is disallowed."); // todo: better error handling!
    }

    states.emplace_back(function);
    begin_scope();
    current_locals().define(function_name, get_current_depth());
    for (const Token &param: stmt.params) {
        current_locals().define(param.get_lexeme(source), get_current_depth());
    }
    visit_stmt(*stmt.body);
    // emit default retrun
    emit(OpCode::NIL);
    emit(OpCode::RETURN);
    end_scope();
    function->set_upvalue_count(states.back().upvalues.size());
    auto upvalues_copy = states.back().upvalues; // todo: elimainate this copy

    // Disassembler disassembler(*function);
    // disassembler.disassemble(function_name);

    states.pop_back();

    int constant = current_function()->add_constant(function);
    emit(OpCode::CLOSURE);
    emit(static_cast<uint8_t>(constant));

    // todo: check!
    for (int i = 0; i < function->get_upvalue_count(); ++i) {
        emit(upvalues_copy[i].is_local);
        emit(upvalues_copy[i].index);
    }
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

int Compiler::add_upvalue(int index, bool is_local, int distance) {
    auto& upvalues = states[states.size() - distance - 1].upvalues;
    Upvalue upvalue {.index = index, .is_local = is_local};
    for (int i = 0; i < upvalues.size(); ++i) {
        if (upvalues[i] == upvalue) {
            return i;
        }
    }

    upvalues.push_back(upvalue);
    return upvalues.size() - 1;
}

int Compiler::resolve_upvalue(const std::string &name, int distance) {
    if (states.size() < distance + 2) {
        return -1;
    }
    int local = states[states.size() - distance - 2].locals.get(name);
    if (local != -1) {
        states[states.size() - distance - 2].locals.close(local);
        return add_upvalue(local, true, distance);
    }

    int upvalue = resolve_upvalue(name, distance + 1);
    if (upvalue != -1) {
        return add_upvalue(upvalue, false, distance);
    }

    return -1;
}

void Compiler::if_statement(const IfStmt &stmt) {
    visit_expr(*stmt.condition);
    auto jump_to_else = start_jump(OpCode::JUMP_IF_FALSE);
    emit(OpCode::POP);
    visit_stmt(*stmt.then_stmt);
    auto jump_to_end = start_jump(OpCode::JUMP);
    jump_to_else.mark_destination(current_program());
    emit(OpCode::POP);
    if (stmt.else_stmt) {
        visit_stmt(*stmt.else_stmt);
    }
    jump_to_end.mark_destination(current_program());
}

void Compiler::while_statement(const WhileStmt &stmt) {
    auto destination = mark_destination();
    visit_expr(*stmt.condition);
    auto jump_to_end = start_jump(OpCode::JUMP_IF_FALSE);
    emit(OpCode::POP);
    visit_stmt(*stmt.stmt);
    emit(OpCode::LOOP);
    destination.make_jump(current_program());
    jump_to_end.mark_destination(current_program());
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

void Compiler::class_declaration(const ClassStmt& stmt) {
    std::string name = stmt.name.get_lexeme(source);
    uint8_t constant = current_function()->add_constant(name);
    current_locals().define(name, get_current_depth());


    if (stmt.super_name) {
        emit(OpCode::GET);
        emit(current_locals().get(stmt.super_name->get_lexeme(source)));

        begin_scope();
        current_locals().define("super", get_current_depth());
    }
    emit(OpCode::CLASS);
    emit(constant);
     if (stmt.super_name) {emit(OpCode::INHERIT); }



    for (auto& method : stmt.methods) {
        // todo: too much repetition with function declaration!!!!
        std::string function_name = std::string(method->name.get_lexeme(source));
        Function *function = new Function(function_name, method->params.size()); // TODO: memory leak!
        allocated_objects.push_back(function);

        states.emplace_back(function);
        begin_scope();
        if (function_name != "init") {
            current_locals().define(function_name, get_current_depth());
        }
        for (const Token &param: method->params) {
            current_locals().define(param.get_lexeme(source), get_current_depth());
        }
        current_locals().define("this", get_current_depth());
        visit_stmt(*method->body);
        // emit default retrun
        if (function_name == "init") {
            emit(OpCode::GET);
            emit(current_locals().get("this"));
        } else {
            emit(OpCode::NIL);
        }

        emit(OpCode::RETURN);
        end_scope();
        function->set_upvalue_count(states.back().upvalues.size());
        auto upvalues_copy = states.back().upvalues; // todo: elimainate this copy
        states.pop_back();
        int constant = current_function()->add_constant(function);
        emit(OpCode::CLOSURE);
        emit(static_cast<uint8_t>(constant));
        for (int i = 0; i < function->get_upvalue_count(); ++i) {
            emit(upvalues_copy[i].is_local);
            emit(upvalues_copy[i].index);
        }

        int idx = current_function()->add_constant(function_name);
        emit(OpCode::METHOD);
        emit(idx);
    }

    if (stmt.super_name) {
        end_scope();
    }
}

void Compiler::get_property(const GetPropertyExpr &expr) {
    visit_expr(*expr.left);
    std::string name = expr.property.get_lexeme(source);
    int constant = current_function()->add_constant(name);
    emit(OpCode::GET_PROPERTY);
    emit(constant);
}

void Compiler::set_property(const SetPropertyExpr &expr) {
    visit_expr(*expr.left);
    std::string name = expr.property.get_lexeme(source);
    int constant = current_function()->add_constant(name);
    visit_expr(*expr.expression);
    emit(OpCode::SET_PROPERTY);
    emit(constant);
}

void Compiler::super(const SuperExpr &expr) {
    int constant = current_function()->add_constant(expr.method.get_lexeme(source));
    emit(OpCode::GET);
    emit(current_locals().get("this"));
    emit(OpCode::GET);
    emit(current_locals().get("super"));
    emit(OpCode::GET_SUPER);
    emit(constant);
}

void Compiler::visit_expr(const Expr &expression) {
    std::visit(overloaded{
                   [this](const LiteralExpr &expr) { literal(expr); },
                   [this](const UnaryExpr &expr) { unary(expr); },
                   [this](const BinaryExpr &expr) { binary(expr); },
                   [this](const StringLiteral &expr) { string_literal(expr); },
                   [this](const VariableExpr &expr) { variable(expr); },
                   [this](const AssigmentExpr &expr) { assigment(expr); },
                   [this](const CallExpr &expr) { call(expr); },
                   [this](const GetPropertyExpr &expr) { get_property(expr); },
                   [this](const SetPropertyExpr &expr) { set_property(expr); },
                   [this](const SuperExpr& expr) {super(expr);}
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
    auto jump = start_jump(expr.op == Token::Type::AND_AND ? OpCode::JUMP_IF_FALSE : OpCode::JUMP_IF_TRUE);
    emit(OpCode::POP);
    visit_expr(*expr.right);
    jump.mark_destination(current_program());
}

void Compiler::string_literal(const StringLiteral &expr) {
    int index = current_function()->add_constant(expr.string); // memory!
    emit(OpCode::CONSTANT);
    emit(index); // handle overflow!!!
}

void Compiler::variable(const VariableExpr &expr) {
    std::string name = expr.identifier.get_lexeme(source);
    int idx = current_locals().get(name);

    if (idx == -1) {
        idx = resolve_upvalue(name, 0);
        assert(idx != -1); // todo: error handling
        emit(OpCode::GET_UPVALUE);
        emit(idx);
    } else {
        emit(OpCode::GET);
        emit(idx); // todo: handle overflow
    }

}

void Compiler::assigment(const AssigmentExpr &expr) {
    std::string name = expr.identifier.get_lexeme(source);
    int idx = current_locals().get(name);
    visit_expr(*expr.expr);
    if (idx == -1) {
        idx = resolve_upvalue(name, 0);
        assert(idx != -1); // todo: error handling
        emit(OpCode::SET_UPVALUE);
        emit(idx);
    } else {
        emit(OpCode::SET);
        emit(idx); // todo: handle overflow
    }
}


void Compiler::call(const CallExpr &expr) {
    visit_expr(*expr.callee);
    for (const ExprHandle &argument: expr.arguments) {
        visit_expr(*argument);
    }
    emit(OpCode::CALL);
    emit(static_cast<uint8_t>(expr.arguments.size()));
}

int &Compiler::get_current_depth() {
    return states.back().current_depth;
}

Function *Compiler::current_function() const {
    return states.back().function;
}

Compiler::Locals& Compiler::current_locals() {
    return states.back().locals;
}

Compiler::JumpDestination Compiler::mark_destination() const {
    return JumpDestination(current_program().size());
}

Compiler::JumpHandle Compiler::start_jump(OpCode op_code) {
    emit(op_code);
    emit(0xFF);
    emit(0xFF);
    return JumpHandle(current_program().size() - 3);
}

int Compiler::Locals::get(const std::string &name) const {
    for (int i = locals.size() - 1; i >= 0; --i) {
        if (locals[i].name == name) return i;
    }
    return -1;
}

bool Compiler::Locals::contains(const std::string &name, int depth) const {
    for (int i = locals.size() - 1; i >= 0; --i) {
        auto& [local_name, local_depth, _] = locals[i];
        if (local_depth < depth) break;

        if (local_name == name) return true;
    }
    return false;
}

bool Compiler::Locals::define(const std::string &name, int depth) {
    if (contains(name, depth)) return false;
    locals.emplace_back(name, depth);
    return true;
}

int Compiler::Locals::erase_above(const int depth) {
    int cnt = 0;
    while (!locals.empty() && locals.back().depth > depth) {
        locals.pop_back();
        ++cnt;
    }
    return cnt;
}

void Compiler::Locals::close(int local) {
    locals[local].is_closed = true;
}

std::vector<Compiler::Local> &Compiler::Locals::get_locals() {
    return locals;
}

void Compiler::JumpDestination::make_jump(Program &program) const {
    int offset = program.size() - position + 2;
    program.write(static_cast<uint8_t>(offset >> 8 & 0xFF));
    program.write(static_cast<uint8_t>(offset & 0xFF));
}

void Compiler::JumpHandle::mark_destination(Program &program) const {
    int offset = program.size() - instruction_position - 3;
    // check jump
    program.patch(instruction_position + 1, static_cast<uint8_t>(offset >> 8 & 0xFF));
    program.patch(instruction_position + 2, static_cast<uint8_t>(offset & 0xFF));
}

void Compiler::compile() {
    for (auto &stmt: parser.parse()) {
        visit_stmt(stmt);
    }
    // default return at main
    emit(OpCode::NIL);
    emit(OpCode::RETURN);
}

 Function &Compiler::get_main()  {
    return main;
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

    auto& locals = current_locals().get_locals();
    while (!locals.empty() && locals.back().depth > get_current_depth()) {
        if (locals.back().is_closed) {
            emit(OpCode::CLOSE_UPVALUE);
        } else {
            emit(OpCode::POP);
        }
        locals.pop_back();
    }
}

Program &Compiler::current_program() const {
    return current_function()->get_program();
}
