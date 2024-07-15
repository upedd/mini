#include "Compiler.h"

#include <cassert>
#include <ranges>

#include "debug.h"

int Compiler::Locals::get(const std::string &name) const {
    for (int i = locals.size() - 1; i >= 0; --i) {
        if (locals[i].name == name) return i;
    }
    return -1;
}

bool Compiler::Locals::contains(const std::string &name, int depth) const {
    for (int i = locals.size() - 1; i >= 0; --i) {
        auto &[local_name, local_depth, _] = locals[i];
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

void Compiler::Locals::close(int local) {
    locals[local].is_closed = true;
}

std::vector<Compiler::Local> &Compiler::Locals::get_locals() {
    return locals;
}

int Compiler::Context::add_upvalue(int index, bool is_local) {
    Upvalue upvalue{.index = index, .is_local = is_local};
    // try to reuse existing upvalue (possible optimization replace linear search)
    auto found = std::ranges::find(upvalues, upvalue);
    if (found != upvalues.end()) {
        return std::distance(upvalues.begin(), found);
    }
    upvalues.push_back(upvalue);
    return upvalues.size() - 1;
}

void Compiler::compile() {
    for (auto &stmt: parser.parse()) {
        for (auto& err : parser.get_errors()) {
            std::cerr << err.message << '\n';
        }
        std::cout << stmt_to_string(stmt, source) << '\n';
        visit_stmt(stmt);
    }
    // default return at main
    emit_default_return();
}

Function &Compiler::get_main() {
    return main;
}

const std::vector<Function *>& Compiler::get_functions() {
    return functions;
}

const std::vector<std::string> & Compiler::get_natives() {
    return natives;
}

void Compiler::start_context(Function *function, FunctionType type) {
    context_stack.emplace_back(function, type);
};

void Compiler::end_context() {
    context_stack.pop_back();
}

Compiler::Context &Compiler::current_context() {
    return context_stack.back();
}

int &Compiler::current_depth() {
    return current_context().current_depth;
}

Function *Compiler::current_function() {
    return current_context().function;
}

Compiler::Locals &Compiler::current_locals() {
    return current_context().locals;
}

Program &Compiler::current_program() {
    return current_function()->get_program();
}

void Compiler::emit(bite_byte byte) {
    current_function()->get_program().write(byte);
}

void Compiler::emit(OpCode op_code) {
    current_function()->get_program().write(op_code);
}

void Compiler::emit(OpCode op_code, bite_byte value) {
    emit(op_code);
    emit(value);
}

void Compiler::emit_default_return() {
    if (current_context().function_type == FunctionType::CONSTRUCTOR) {
        emit(OpCode::GET);
        emit(current_locals().get("this"));
    } else {
        emit(OpCode::NIL);
    }
    emit(OpCode::RETURN);
}

void Compiler::begin_scope() {
    ++current_depth();
}

void Compiler::end_scope() {
    --current_depth();

    auto &locals = current_locals().get_locals();
    while (!locals.empty() && locals.back().depth > current_depth()) {
        if (locals.back().is_closed) {
            emit(OpCode::CLOSE_UPVALUE);
        } else {
            emit(OpCode::POP);
        }
        locals.pop_back();
    }
}

void Compiler::define_variable(const std::string &name) {
    if (!current_locals().define(name, current_depth())) {
        throw Error("Variable redefinition in same scope is disallowed.");
    }
}

void Compiler::resolve_variable(const std::string &name) {
    int idx = current_locals().get(name);

    if (idx == -1) {
        idx = resolve_upvalue(name);
        assert(idx != -1); // todo: error handling
        emit(OpCode::GET_UPVALUE);
        emit(idx);
    } else {
        emit(OpCode::GET);
        emit(idx); // todo: handle overflow
    }
}

int Compiler::resolve_upvalue(const std::string &name) {
    int resolved = -1;
    // find first context that contains given name as a local variable
    // while tracking all contexts that we needed to go through while getting to that local
    std::vector<std::reference_wrapper<Context> > resolve_up;
    for (Context &context: context_stack | std::views::reverse) {
        resolved = context.locals.get(name);
        if (resolved != -1) {
            context.locals.close(resolved);
            break;
        }
        resolve_up.emplace_back(context);
    }
    // if we didn't find any local variable with given name return -1;
    if (resolved == -1) return resolved;
    // tracks whetever upvalue points to local value or another upvalue
    bool is_local = true;
    // go from context that contains local variable to context that we are resolving from
    for (std::reference_wrapper<Context> context: resolve_up | std::views::reverse) {
        resolved = context.get().add_upvalue(resolved, is_local);
        if (is_local) is_local = false; // only top level context points to local variable
    }

    return resolved;
}

void Compiler::visit_stmt(const Stmt &statement) {
    std::visit(overloaded{
                   [this](const VarStmt &stmt) { variable_declaration(stmt); },
                   [this](const FunctionStmt &stmt) { function_declaration(stmt); },
                   [this](const ExprStmt &stmt) { expr_statement(stmt); },
                   [this](const WhileStmt &stmt) { while_statement(stmt); },
                   [this](const ReturnStmt &stmt) { return_statement(stmt); },
                   [this](const ClassStmt &stmt) { class_declaration(stmt); },
                   [this](const NativeStmt& stmt) {native_declaration(stmt); }
               }, statement);
}

void Compiler::variable_declaration(const VarStmt &expr) {
    auto name = expr.name.get_lexeme(source);
    visit_expr(*expr.value);
    define_variable(name);
}

void Compiler::function_declaration(const FunctionStmt &stmt) {
    define_variable(stmt.name.get_lexeme(source));
    function(stmt, FunctionType::FUNCTION);
}

void Compiler::native_declaration(const NativeStmt &stmt) {
    std::string name = stmt.name.get_lexeme(source);
    natives.push_back(name);
    int idx = current_function()->add_constant(name);
    emit(OpCode::GET_NATIVE, idx);
    define_variable(name);
}

void Compiler::block(const BlockExpr &expr) {
    begin_scope();
    for (auto& stmt : expr.stmts) {
        visit_stmt(*stmt);
    }
    if (expr.expr) {
        visit_expr(*expr.expr);
    } else {
        emit(OpCode::NIL);
    }
    end_scope();
}

void Compiler::loop_expression(const LoopExpr &expr) {
    int continue_idx = current_function()->add_jump_destination(current_program().size());
    emit(OpCode::PUSH_BLOCK);
    int loop_idx = current_function()->add_jump_destination(current_program().size());
    int break_idx = current_function()->add_empty_jump_destination();
    current_context().blocks.emplace_back(break_idx, continue_idx);
    emit(OpCode::PUSH_BLOCK);
    visit_expr(*expr.body);
    emit(OpCode::POP); // ignore expressions result
    emit(OpCode::JUMP, loop_idx);
    current_function()->patch_jump_destination(current_context().blocks.back().break_jump_idx, current_program().size());
    current_context().blocks.pop_back();
    emit(OpCode::POP_BLOCK);
}

void Compiler::break_expr(const BreakExpr &expr) {
    // TODO: parser should check if break expressions actually in loop
    if (expr.expr) {
        visit_expr(*expr.expr);
    } else {
        emit(OpCode::NIL);
    }
    emit(OpCode::JUMP, current_context().blocks.back().break_jump_idx);
}

void Compiler::continue_expr(const ContinueExpr& expr) {
    emit(OpCode::NIL);
    emit(OpCode::POP_BLOCK);
    emit(OpCode::POP);
    emit(OpCode::JUMP, current_context().blocks.back().continue_jump_idx);
}

void Compiler::function(const FunctionStmt &stmt, FunctionType type) {
    auto function_name = stmt.name.get_lexeme(source);
    auto *function = new Function(function_name, stmt.params.size());
    functions.push_back(function);
    //current_function()->add_allocated(function);

    start_context(function, type);
    begin_scope();
    if (type == FunctionType::METHOD || type == FunctionType::CONSTRUCTOR) {
        current_locals().define("this", current_depth());
    } else {
        current_locals().define("", current_depth());
    }
    for (const Token &param: stmt.params) {
        define_variable(param.get_lexeme(source));
    }

    visit_expr(*stmt.body);
    emit_default_return();
    end_scope();

    function->set_upvalue_count(current_context().upvalues.size());
    // we need to emit those upvalues in enclosing context (context where function is called)
    std::vector<Upvalue> function_upvalues = std::move(current_context().upvalues);
    end_context();

    int constant = current_function()->add_constant(function);
    emit(OpCode::CLOSURE, constant);

    for (const Upvalue &upvalue: function_upvalues) {
        emit(upvalue.is_local);
        emit(upvalue.index);
    }
}

void Compiler::class_declaration(const ClassStmt &stmt) {
    std::string name = stmt.name.get_lexeme(source);
    uint8_t constant = current_function()->add_constant(name);
    current_locals().define(name, current_depth());

    emit(OpCode::CLASS);
    emit(constant);

    if (stmt.super_name) {
        emit(OpCode::GET);
        emit(current_locals().get(stmt.super_name->get_lexeme(source)));
        emit(OpCode::GET);
        emit(current_locals().get(name));
        emit(OpCode::INHERIT);
        begin_scope();
        current_locals().define("super", current_depth());
    }
    emit(OpCode::GET);
    emit(current_locals().get(name));
    for (auto &method: stmt.methods) {
        function(*method, FunctionType::METHOD);
        int idx = current_function()->add_constant(method->name.get_lexeme(source));
        emit(OpCode::METHOD);
        emit(idx);
    }
    emit(OpCode::POP);

    if (stmt.super_name) {
        end_scope();
    }
}

void Compiler::expr_statement(const ExprStmt &stmt) {
    visit_expr(*stmt.expr);
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

void Compiler::while_statement(const WhileStmt &stmt) {
    int destination = current_function()->add_jump_destination(current_program().size());
    visit_expr(*stmt.condition);
    int jump_to_end = current_function()->add_empty_jump_destination();
    emit(OpCode::JUMP_IF_FALSE, jump_to_end);
    emit(OpCode::POP);
    visit_stmt(*stmt.stmt);
    emit(OpCode::JUMP, destination);
    current_function()->patch_jump_destination(jump_to_end, current_program().size());
    emit(OpCode::POP);
}

void Compiler::if_expression(const IfExpr &stmt) {
    visit_expr(*stmt.condition);
    int jump_to_else = current_function()->add_empty_jump_destination();
    emit(OpCode::JUMP_IF_FALSE, jump_to_else);
    emit(OpCode::POP);
    visit_expr(*stmt.then_expr);
    auto jump_to_end = current_function()->add_empty_jump_destination();
    emit(OpCode::JUMP, jump_to_end);
    current_function()->patch_jump_destination(jump_to_else, current_program().size());
    emit(OpCode::POP);
    if (stmt.else_expr) {
        visit_expr(*stmt.else_expr);
    } else {
        emit(OpCode::NIL);
    }
    current_function()->patch_jump_destination(jump_to_end, current_program().size());
}

void Compiler::visit_expr(const Expr &expression) {
    std::visit(overloaded{
                   [this](const LiteralExpr &expr) { literal(expr); },
                   [this](const UnaryExpr &expr) { unary(expr); },
                   [this](const BinaryExpr &expr) { binary(expr); },
                   [this](const StringLiteral &expr) { string_literal(expr); },
                   [this](const VariableExpr &expr) { variable(expr); },
                   [this](const CallExpr &expr) { call(expr); },
                   [this](const GetPropertyExpr &expr) { get_property(expr); },
                   [this](const SuperExpr &expr) { super(expr); },
                   [this](const BlockExpr& expr) {block(expr);},
                   [this](const IfExpr& expr) {if_expression(expr);},
                   [this](const LoopExpr& expr) {loop_expression(expr);},
                   [this](const BreakExpr& expr) {break_expr(expr);},
                   [this](const ContinueExpr& expr) {continue_expr(expr);}
               }, expression);
}

void Compiler::literal(const LiteralExpr &expr) {
    int index = current_function()->add_constant(expr.literal);
    emit(OpCode::CONSTANT);
    emit(index); // handle overflow!!!
}

void Compiler::string_literal(const StringLiteral &expr) {
    int index = current_function()->add_constant(expr.string); // memory!
    emit(OpCode::CONSTANT, index); // handle overflow!!!
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
        default: assert("unreachable");
    }
}

void Compiler::binary(const BinaryExpr &expr) {
    // we don't need to actually visit lhs for plain assigment
    if (expr.op == Token::Type::EQUAL) {
        visit_expr(*expr.right);
        update_lvalue(*expr.left);
        return;
    }

    visit_expr(*expr.left);
    // we need handle logical expressions before we execute right side as they can short circut
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
        case Token::Type::PLUS_EQUAL:
            emit(OpCode::ADD);
            update_lvalue(*expr.left);
        break;
        case Token::Type::MINUS_EQUAL:
            emit(OpCode::SUBTRACT);
            update_lvalue(*expr.left);
        break;
        case Token::Type::STAR_EQUAL:
            emit(OpCode::MULTIPLY);
            update_lvalue(*expr.left);
        break;
        case Token::Type::SLASH_EQUAL:
            emit(OpCode::DIVIDE);
            update_lvalue(*expr.left);
        break;
        case Token::Type::SLASH_SLASH_EQUAL:
            emit(OpCode::FLOOR_DIVISON);
            update_lvalue(*expr.left);
        break;
        case Token::Type::PERCENT_EQUAL:
            emit(OpCode::MODULO);
            update_lvalue(*expr.left);
        break;
        case Token::Type::LESS_LESS_EQUAL:
            emit(OpCode::LEFT_SHIFT);
            update_lvalue(*expr.left);
        break;
        case Token::Type::GREATER_GREATER_EQUAL:
            emit(OpCode::RIGHT_SHIFT);
            update_lvalue(*expr.left);
        break;
        case Token::Type::AND_EQUAL:
            emit(OpCode::BITWISE_AND);
            update_lvalue(*expr.left);
        break;
        case Token::Type::CARET_EQUAL:
            emit(OpCode::BITWISE_XOR);
            update_lvalue(*expr.left);
        break;
        case Token::Type::BAR_EQUAL:
            emit(OpCode::BITWISE_OR);
            update_lvalue(*expr.left);
        break;
        default: assert("unreachable");
    }
}


void Compiler::update_lvalue(const Expr& lvalue) {
    if (std::holds_alternative<VariableExpr>(lvalue)) {
        std::string name = std::get<VariableExpr>(lvalue).identifier.get_lexeme(source);
        int idx = current_locals().get(name);
        if (idx == -1) {
            idx = resolve_upvalue(name);
            assert(idx != -1); // todo: error handling
            emit(OpCode::SET_UPVALUE);
            emit(idx);
        } else {
            emit(OpCode::SET);
            emit(idx); // todo: handle overflow
        }
    } else if (std::holds_alternative<GetPropertyExpr>(lvalue)) {
        const auto& property_expr = std::get<GetPropertyExpr>(lvalue);
        std::string name = property_expr.property.get_lexeme(source);
        int constant = current_function()->add_constant(name);
        visit_expr(*property_expr.left);
        emit(OpCode::SET_PROPERTY, constant);
    } else {
        assert("Expected lvalue.");
    }
}

void Compiler::variable(const VariableExpr &expr) {
    std::string name = expr.identifier.get_lexeme(source);
    resolve_variable(name);
}

void Compiler::logical(const BinaryExpr &expr) {
    int jump = current_function()->add_empty_jump_destination();
    emit(expr.op == Token::Type::AND_AND ? OpCode::JUMP_IF_FALSE : OpCode::JUMP_IF_TRUE, jump);
    emit(OpCode::POP);
    visit_expr(*expr.right);
    current_function()->patch_jump_destination(jump, current_program().size());
}

void Compiler::call(const CallExpr &expr) {
    visit_expr(*expr.callee);
    for (const ExprHandle &argument: expr.arguments) {
        visit_expr(*argument);
    }
    emit(OpCode::CALL, expr.arguments.size());
}

void Compiler::get_property(const GetPropertyExpr &expr) {
    visit_expr(*expr.left);
    std::string name = expr.property.get_lexeme(source);
    int constant = current_function()->add_constant(name);
    emit(OpCode::GET_PROPERTY, constant);
}

void Compiler::super(const SuperExpr &expr) {
    int constant = current_function()->add_constant(expr.method.get_lexeme(source));
    resolve_variable("this");
    resolve_variable("super");
    emit(OpCode::GET_SUPER, constant);
}
