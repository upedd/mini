#include "Compiler.h"

#include <cassert>
#include <ranges>

#include "debug.h"

void Compiler::Scope::mark_temporary(int count) {
    temporaries += count;
}

void Compiler::Scope::pop_temporary(int count) {
    temporaries -= count;
}

int Compiler::Scope::define(const std::string &name) {
    locals.emplace_back(name);
    //++items_on_stack;
    return slot_start + temporaries;
}

std::optional<int> Compiler::Scope::get(const std::string &name) {
    // assumes locals are not mixed with temporaries in scope is that true?
    for (int i = 0; i < locals.size(); ++i) {
        if (locals[i].name == name) {
            return slot_start + i;
        }
    }
    return {};
}

void Compiler::Scope::close(int index) {
    assert(index < locals.size());
    locals[index].is_closed = true;
}

int Compiler::Scope::next_slot() {
    return slot_start + locals.size() + temporaries;
}

Compiler::Context::Resolution Compiler::Context::resolve_variable(const std::string &name) {
    for (auto &scope: std::views::reverse(scopes)) {
        if (scope.get_type() == ScopeType::CLASS && scope.has_field(name)) {
            return FieldResolution();
        }
        if (auto index = scope.get(name)) {
            // resolve local
            return LocalResolution(*index);
        }
    }
    return std::monostate();
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

void Compiler::Context::close_upvalue(int index) {
    for (auto &scope: std::views::reverse(scopes)) {
        if (scope.get_start_slot() <= index) {
            scope.close(index - scope.get_start_slot());
            break;
        }
    }
}

void Compiler::compile() {
    for (auto &stmt: parser.parse()) {
        for (auto &err: parser.get_errors()) {
            std::cerr << err.message << '\n';
        }
        //std::cout << stmt_to_string(stmt, source) << '\n';
        visit_stmt(stmt);
    }
    // default return at main
    emit_default_return();
}

Function &Compiler::get_main() {
    return main;
}

const std::vector<Function *> &Compiler::get_functions() {
    return functions;
}

const std::vector<std::string> &Compiler::get_natives() {
    return natives;
}

void Compiler::this_expr() {
    // safety: check if used in class method context
    emit(OpCode::THIS);
}

void Compiler::start_context(Function *function, FunctionType type) {
    context_stack.emplace_back(function, type);
    current_context().scopes.emplace_back(ScopeType::BLOCK, 0); // TODO: should be function?
}

//#define COMPILER_PRINT_BYTECODE

void Compiler::end_context() {
#ifdef COMPILER_PRINT_BYTECODE
    Disassembler disassembler(*current_function());
    disassembler.disassemble(current_function()->to_string());
#endif

    context_stack.pop_back();
}

Compiler::Context &Compiler::current_context() {
    return context_stack.back();
}

Compiler::Scope &Compiler::current_scope() {
    return current_context().current_scope();
}


Function *Compiler::current_function() {
    return current_context().function;
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
        emit(OpCode::THIS);
    } else {
        emit(OpCode::NIL);
    }
    emit(OpCode::RETURN);
}

void Compiler::begin_scope(ScopeType type, const std::string &label) {
    current_context().scopes.emplace_back(type, current_scope().next_slot(), label);
}

void Compiler::pop_out_of_scopes(int depth) {
    for (int i = 0; i < depth; ++i) {
        Scope &scope = current_context().scopes[current_context().scopes.size() - i - 1];
        // every scope in bite is an expression which should produce a value
        // so we actually leave last value on the stack
        // every time we begin scope we have to remeber that (TODO: maybe this system could be rewritten more clearly)
        for (int j = 0; j < scope.get_temporaries_count(); ++j) {
            emit(OpCode::POP);
        }
        bool leave_last = i == depth - 1;
        auto &locals = scope.get_locals();
        for (int j = locals.size() - 1; j >= leave_last; --j) {
            if (locals[j].is_closed) {
                emit(OpCode::CLOSE_UPVALUE);
            } else {
                emit(OpCode::POP);
            }
        }
    }
}

void Compiler::end_scope() {
    if (current_scope().get_type() == ScopeType::CLASS) {
        current_context().resolved_classes[current_scope().get_name()] = ResolvedClass(
            current_scope().get_fields(), current_scope().constructor_argument_count);
    }
    pop_out_of_scopes(1);
    current_context().scopes.pop_back();
    current_scope().mark_temporary();
}

void Compiler::define_variable(const std::string &name) {
    if (current_scope().get(name)) {
        throw Error("Variable redefinition in same scope is disallowed.");
    }
    current_scope().define(name);
}

// TODO: total mess!!!!
void Compiler::resolve_variable(const std::string &name) {
    auto resolution = current_context().resolve_variable(name);
    if (std::holds_alternative<Context::LocalResolution>(resolution)) {
        emit(OpCode::GET, std::get<Context::LocalResolution>(resolution).slot); // todo: handle overflow
    } else if (std::holds_alternative<Context::FieldResolution>(resolution)) {
        // TODO: this doesn't work with nested classes i think?
        emit(OpCode::THIS);
        emit(OpCode::GET_PROPERTY, current_function()->add_constant(name));
    } else {
        auto up_resolution = resolve_upvalue(name);
        if (std::holds_alternative<Context::LocalResolution>(up_resolution)) {
            emit(OpCode::GET_UPVALUE, std::get<Context::LocalResolution>(up_resolution).slot); // todo: handle overflow
        } else if (std::holds_alternative<Context::FieldResolution>(up_resolution)) {
            // TODO: this doesn't work with nested classes i think?
            emit(OpCode::THIS);
            emit(OpCode::GET_PROPERTY, current_function()->add_constant(name));
        } else {
            assert("unreachable!");
        }
    }
    current_scope().mark_temporary();
}

Compiler::Context::Resolution Compiler::resolve_upvalue(const std::string &name) {
    std::optional<int> resolved;
    // find first context that contains given name as a local variable
    // while tracking all contexts that we needed to go through while getting to that local
    std::vector<std::reference_wrapper<Context> > resolve_up;
    for (Context &context: context_stack | std::views::reverse) {
        auto resolution = context.resolve_variable(name);
        if (std::holds_alternative<Context::LocalResolution>(resolution)) {
            resolved = std::get<Context::LocalResolution>(resolution).slot;
            // TODO: fix upvalues!
            context.close_upvalue(*resolved);
            break;
        }
        if (std::holds_alternative<Context::FieldResolution>(resolution)) {
            return resolution;
        }
        resolve_up.emplace_back(context);
    }
    // if we didn't find any local variable with given name return -1;
    if (!resolved) return std::monostate();
    // tracks whetever upvalue points to local value or another upvalue
    bool is_local = true;
    // go from context that contains local variable to context that we are resolving from
    for (std::reference_wrapper<Context> context: resolve_up | std::views::reverse) {
        resolved = context.get().add_upvalue(*resolved, is_local);
        if (is_local) is_local = false; // only top level context points to local variable
    }

    return Context::LocalResolution(*resolved);
}

void Compiler::visit_stmt(const Stmt &statement) {
    std::visit(overloaded{
                   [this](const VarStmt &stmt) { variable_declaration(stmt); },
                   [this](const FunctionStmt &stmt) { function_declaration(stmt); },
                   [this](const ExprStmt &stmt) { expr_statement(stmt); },
                   [this](const ClassStmt &stmt) { class_declaration(stmt); },
                   [this](const NativeStmt &stmt) { native_declaration(stmt); },
                   [this](const MethodStmt &) { assert("unreachable"); },
                   [this](const FieldStmt &) { assert("unreachable"); },
                   [this](const ConstructorStmt &) { assert("unreachable"); }
               }, statement);
}

void Compiler::variable_declaration(const VarStmt &expr) {
    auto name = expr.name.get_lexeme(source);
    visit_expr(*expr.value);
    current_scope().pop_temporary();
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
    int break_idx = current_function()->add_empty_jump_destination();
    begin_scope(ScopeType::BLOCK, expr.label ? expr.label->get_lexeme(source) : "");
    current_scope().break_idx = break_idx;
    emit(OpCode::NIL);
    define_variable("$scope_return");
    current_scope().return_slot = *current_scope().get("$scope_return");
    for (auto &stmt: expr.stmts) {
        visit_stmt(*stmt);
    }
    if (expr.expr) {
        visit_expr(*expr.expr);
        emit(OpCode::SET, current_scope().return_slot);
        emit(OpCode::POP);
        current_scope().pop_temporary();
    }
    end_scope();
    current_function()->patch_jump_destination(break_idx, current_program().size());
}

void Compiler::loop_expression(const LoopExpr &expr) {
    std::string label = expr.label ? expr.label->get_lexeme(source) : "";
    begin_scope(ScopeType::LOOP, label);
    // to support breaking with values before loop body we create special invisible variable used for returing
    emit(OpCode::NIL);
    define_variable("$scope_return");
    current_scope().return_slot = std::get<Context::LocalResolution>(
        current_context().resolve_variable("$scope_return")).slot;
    int continue_idx = current_function()->add_jump_destination(current_program().size());
    int break_idx = current_function()->add_empty_jump_destination();
    current_scope().continue_idx = continue_idx;
    current_scope().break_idx = break_idx;
    visit_expr(*expr.body);
    // ignore body expression result
    emit(OpCode::POP);
    current_scope().pop_temporary();

    emit(OpCode::JUMP, continue_idx);
    end_scope();
    current_function()->patch_jump_destination(break_idx, current_program().size());
}

void Compiler::while_expr(const WhileExpr &expr) {
    // Tons of overlap with normal loop maybe abstract this away?
    std::string label = expr.label ? expr.label->get_lexeme(source) : "";
    begin_scope(ScopeType::LOOP, label);
    emit(OpCode::NIL);
    define_variable("$scope_return");
    current_scope().return_slot = std::get<Context::LocalResolution>(
        current_context().resolve_variable("$scope_return")).slot;

    int continue_idx = current_function()->add_jump_destination(current_program().size());
    int break_idx = current_function()->add_empty_jump_destination();
    int end_idx = current_function()->add_empty_jump_destination();
    current_scope().continue_idx = continue_idx;
    current_scope().break_idx = break_idx;

    visit_expr(*expr.condition);
    emit(OpCode::JUMP_IF_FALSE, end_idx);
    emit(OpCode::POP); // pop evaluation condition result
    current_scope().pop_temporary();
    visit_expr(*expr.body);
    // ignore block expression result
    emit(OpCode::POP);
    current_scope().pop_temporary();
    emit(OpCode::JUMP, continue_idx);
    end_scope();
    current_function()->patch_jump_destination(end_idx, current_program().size());
    emit(OpCode::POP); // pop evalutaion condition result
    current_function()->patch_jump_destination(break_idx, current_program().size());
}

void Compiler::for_expr(const ForExpr &expr) {
    // Tons of overlap with while loop maybe abstract this away?
    // ideally some sort of desugaring step
    begin_scope(ScopeType::BLOCK);
    emit(OpCode::NIL);
    define_variable("$scope_return");
    // begin define iterator
    visit_expr(*expr.iterable);
    int iterator_constant = current_function()->add_constant("iterator");
    emit(OpCode::GET_PROPERTY, iterator_constant);
    emit(OpCode::CALL, 0);
    define_variable("$iterator");
    current_scope().pop_temporary();
    // end define iterator
    std::string label = expr.label ? expr.label->get_lexeme(source) : "";
    begin_scope(ScopeType::LOOP, label);
    emit(OpCode::NIL);
    define_variable("$scope_return");
    current_scope().return_slot = std::get<Context::LocalResolution>(
        current_context().resolve_variable("$scope_return")).slot;
    int continue_idx = current_function()->add_jump_destination(current_program().size());
    int break_idx = current_function()->add_empty_jump_destination();
    int end_idx = current_function()->add_empty_jump_destination();
    current_scope().continue_idx = continue_idx;
    current_scope().break_idx = break_idx;

    // begin condition
    resolve_variable("$iterator");
    int condition_cosntant = current_function()->add_constant("has_next");
    emit(OpCode::GET_PROPERTY, condition_cosntant);
    emit(OpCode::CALL, 0);
    // end condition

    emit(OpCode::JUMP_IF_FALSE, end_idx);
    emit(OpCode::POP); // pop evaluation condition result
    current_scope().pop_temporary();

    // begin item
    resolve_variable("$iterator");
    int item_constant = current_function()->add_constant("next");
    emit(OpCode::GET_PROPERTY, item_constant);
    emit(OpCode::CALL, 0);
    define_variable(expr.name.get_lexeme(source));
    current_scope().pop_temporary();
    // end item

    visit_expr(*expr.body);
    // ignore block expression result
    emit(OpCode::POP);
    current_scope().pop_temporary();
    pop_out_of_scopes(1);
    emit(OpCode::JUMP, continue_idx);
    end_scope();
    current_function()->patch_jump_destination(end_idx, current_program().size());
    emit(OpCode::POP); // pop evalutaion condition result
    current_function()->patch_jump_destination(break_idx, current_program().size());
    emit(OpCode::SET, *current_scope().get("$scope_return"));
    emit(OpCode::POP);
    current_scope().pop_temporary();
    end_scope();
}


void Compiler::break_expr(const BreakExpr &expr) {
    // TODO: parser should check if break expressions actually in loop
    // TODO: better way to write this
    int scope_depth = 0;
    for (auto &scope: std::views::reverse(current_context().scopes)) {
        if (expr.label) {
            if (scope.get_name() == expr.label->get_lexeme(source)) {
                break;
            }
        } else if (scope.get_type() == ScopeType::LOOP) {
            // We want unlabeled breaks to break only from loops
            break;
        }
        ++scope_depth;
    }
    Scope &scope = current_context().scopes[current_context().scopes.size() - scope_depth - 1];
    if (expr.expr) {
        visit_expr(*expr.expr);
        emit(OpCode::SET, scope.return_slot);
        emit(OpCode::POP);
        current_scope().pop_temporary();
    }
    pop_out_of_scopes(scope_depth + 1);
    emit(OpCode::JUMP, scope.break_idx);
}

void Compiler::continue_expr(const ContinueExpr &expr) {
    // safety: assert that contains label?
    int scope_depth = 0;
    for (auto &scope: std::views::reverse(current_context().scopes)) {
        if (scope.get_type() == ScopeType::LOOP) {
            // can only continue from loops
            if (expr.label) {
                if (expr.label->get_lexeme(source) == scope.get_name()) {
                    break;
                }
            } else break;
        }
        ++scope_depth;
    }
    Scope &scope = current_context().scopes[current_context().scopes.size() - scope_depth - 1];
    pop_out_of_scopes(scope_depth + 1);
    emit(OpCode::JUMP, scope.continue_idx);
}


void Compiler::function(const FunctionStmt &stmt, FunctionType type) {
    auto function_name = stmt.name.get_lexeme(source);
    auto *function = new Function(function_name, stmt.params.size());
    functions.push_back(function);

    start_context(function, type);
    begin_scope(ScopeType::BLOCK);
    current_scope().define(""); // reserve slot for receiver
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

void Compiler::constructor(const ConstructorStmt &stmt, const std::vector<std::unique_ptr<FieldStmt> > &fields,
                           bool has_superclass, int superclass_arguments_count) {
    // refactor: tons of overlap with function generator

    // TODO: check name
    auto *function = new Function("constructor", stmt.parameters.size());
    functions.push_back(function);
    start_context(function, FunctionType::CONSTRUCTOR);
    begin_scope(ScopeType::BLOCK); // doesn't context already start a new scope therefor it is reduntant
    current_scope().define(""); // reserve slot for receiver

    for (const Token &param: stmt.parameters) {
        define_variable(param.get_lexeme(source));
    }

    if (stmt.has_super) {
        if (!has_superclass) {
            assert(false && "No superclass to be constructed");
        }
        for (const ExprHandle &expr: stmt.super_arguments) {
            visit_expr(*expr);
        }
        // maybe better way to do this instead of this superinstruction?
        emit(OpCode::CALL_SUPER_CONSTRUCTOR, stmt.super_arguments.size());
        emit(OpCode::POP); // discard constructor response
    } else if (has_superclass) {
        if (superclass_arguments_count == 0) {
            // default superclass construct
            emit(OpCode::CALL_SUPER_CONSTRUCTOR, stmt.super_arguments.size());
            emit(OpCode::POP); // discard constructor response
        } else {
            assert(false && "expected superconstructor call");
        }
    }


    // default initialize fields
    for (auto &field: fields) {
        // ast builder
        if (field->attributes[ClassAttributes::STATIC] || field->attributes[ClassAttributes::ABSTRACT]) continue;
        visit_expr(*field->variable->value);
        emit(OpCode::THIS);
        int property_name = current_function()->add_constant(field->variable->name.get_lexeme(source));
        emit(OpCode::SET_PROPERTY, property_name);
        emit(OpCode::POP); // pop value;
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

void Compiler::default_constructor(const std::vector<std::unique_ptr<FieldStmt> > &fields, bool has_superclass) {
    // TODO: ideally in future default constructor has just default parameters!
    auto *function = new Function("constructor", 0);
    functions.push_back(function);
    start_context(function, FunctionType::CONSTRUCTOR);
    begin_scope(ScopeType::BLOCK); // doesn't context already start a new scope therefor it is reduntant
    current_scope().define(""); // reserve slot for receiver

    if (has_superclass) {
        // default superclass construct
        emit(OpCode::CALL_SUPER_CONSTRUCTOR, 0);
        emit(OpCode::POP); // discard constructor response
    }

    // default initialize fields
    for (auto &field: fields) {
        // ast builder
        if (field->is_static || field->is_abstract) continue;
        visit_expr(*field->variable->value);
        emit(OpCode::THIS);
        int property_name = current_function()->add_constant(field->variable->name.get_lexeme(source));
        emit(OpCode::SET_PROPERTY, property_name);
        emit(OpCode::POP); // pop value;
    }

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
    // TODO: refactor!
    std::string name = stmt.name.get_lexeme(source);
    uint8_t name_constant = current_function()->add_constant(name);
    if (stmt.is_abstract) {
        emit(OpCode::ABSTRACT_CLASS, name_constant);
    } else {
        emit(OpCode::CLASS, name_constant);
    }
    current_scope().define(name);

    begin_scope(ScopeType::CLASS, name);
    // TODO: non-expression scope?
    emit(OpCode::NIL);
    define_variable("$scope_return");

    //std::unordered_map<std::string, FieldInfo>* super_fields = nullptr;
    std::unordered_set<std::string> current_fields;
    if (stmt.super_class) {
        std::string super_class_name = stmt.super_class->get_lexeme(source);
        emit(OpCode::GET,
             std::get<Context::LocalResolution>(current_context().resolve_variable(super_class_name)).slot);
        emit(OpCode::GET, std::get<Context::LocalResolution>(current_context().resolve_variable(name)).slot);
        emit(OpCode::INHERIT);
        emit(OpCode::POP);
        assert(current_context().resolved_classes.contains(super_class_name));
        auto super_fields = current_context().resolved_classes[super_class_name].fields;
        for (auto &field: super_fields) {
            // TODO: private fields should not even be included!
            //if (field.second.is_private) continue;
            current_scope().add_field(field.first, field.second);
        }
    }

    emit(OpCode::GET, std::get<Context::LocalResolution>(current_context().resolve_variable(name)).slot);
    current_scope().mark_temporary();


    // TODO: disallow init keyword!
    for (auto &field: stmt.fields) {
        std::string field_name = field->variable->name.get_lexeme(source);
        if (field->is_static) {
            visit_expr(*field->variable->value);
        }
        int idx = current_function()->add_constant(field_name);
        emit(OpCode::FIELD, idx);
        emit(field->is_private | (field->is_static << 1) | (field->is_abstract << 2));
        if (current_fields.contains(field_name)) {
            // TODO: better error handling!!!
            assert(false && "Field redefnintion is disallowed.");
        }
        bool is_overriding = false;
        if (current_scope().has_field(field_name)) {
            if (!current_scope().get_field_info(field_name).is_private && !current_scope().get_field_info(field_name).
                is_static) {
                // non-private!
                is_overriding = true;
                if (!field->is_override)
                    assert(false && "override expected");
            }
        }
        if (!is_overriding && field->is_override)
            assert(false && "no member to override.");
        if (field->is_static) {
            current_scope().pop_temporary();
        }
        current_scope().add_field(field->variable->name.get_lexeme(source), {
                                      .is_private = field->is_private, .is_static = field->is_static,
                                      .is_override = field->is_override, .is_abstract = field->is_abstract
                                  });
        current_fields.insert(field_name);
    }

    // hoist methods
    for (auto &method: stmt.methods) {
        std::string method_name = method->function->name.get_lexeme(source);
        bool is_overriding = false;
        if (current_scope().has_field(method_name)) {
            if (!current_scope().get_field_info(method_name).is_private && !current_scope().get_field_info(method_name).
                is_static) {
                // non-private!
                is_overriding = true;
                if (!method->is_override)
                    assert(false && "override expected");
            }
        }
        if (!is_overriding && method->is_override) {
            assert(false && "no member to override.");
        }
        if (current_fields.contains(method_name)) {
            // TODO: better error handling!!!
            assert(false && "Field redefnintion is disallowed.");
        }
        current_scope().add_field(method_name, {
                                      .is_private = method->is_private, .is_static = method->is_static,
                                      .is_override = method->is_override, .is_abstract = method->is_abstract
                                  });
        current_fields.insert(method_name);
    }

    for (auto &method: stmt.methods) {
        std::string name = method->function->name.get_lexeme(source);
        if (!method->is_abstract) {
            function(*method->function, FunctionType::METHOD);
        }
        int idx = current_function()->add_constant(name);
        emit(OpCode::METHOD, idx);
        emit(method->is_private | (method->is_static << 1) | (method->is_abstract << 2));
        current_scope().pop_temporary();
    }

    // check if all abstract classes are overriden
    if (!stmt.is_abstract && stmt.super_class) {
        for (auto &field: current_context().resolved_classes[stmt.super_class->get_lexeme(source)].fields) {
            if (field.second.is_abstract && !current_fields.contains(field.first)) {
                assert(false && "Expected abstract override");
            }
        }
    }

    if (stmt.constructor) {
        current_scope().constructor_argument_count = stmt.constructor->parameters.size();
        constructor(*stmt.constructor, stmt.fields, static_cast<bool>(stmt.super_class),
                    current_context().resolved_classes[stmt.super_class->get_lexeme(source)].
                    constructor_argument_count);
    } else {
        if (stmt.super_class && current_context().resolved_classes[stmt.super_class->get_lexeme(source)].
            constructor_argument_count != 0) {
            assert(false && "Class must implement constructor because it needs to call superclass constructor");
        }
        default_constructor(stmt.fields, static_cast<bool>(stmt.super_class));
    }
    emit(OpCode::CONSTRUCTOR);

    emit(OpCode::POP);
    current_scope().pop_temporary();
    // TODO: non-expression scope?
    end_scope();
    emit(OpCode::POP);
    current_scope().pop_temporary();
    // if (stmt.super_name) {
    //     end_scope();
    // }
}

void Compiler::expr_statement(const ExprStmt &stmt) {
    visit_expr(*stmt.expr);

    emit(OpCode::POP);
    current_scope().pop_temporary();
}

void Compiler::retrun_expression(const ReturnExpr &stmt) {
    if (stmt.value != nullptr) {
        visit_expr(*stmt.value);
    } else {
        emit(OpCode::NIL);
        current_scope().mark_temporary();
    }
    emit(OpCode::RETURN);
    emit(OpCode::NIL);
    current_scope().mark_temporary();
}

void Compiler::if_expression(const IfExpr &stmt) {
    visit_expr(*stmt.condition);
    int jump_to_else = current_function()->add_empty_jump_destination();
    emit(OpCode::JUMP_IF_FALSE, jump_to_else);
    emit(OpCode::POP);
    current_scope().pop_temporary();
    visit_expr(*stmt.then_expr);
    auto jump_to_end = current_function()->add_empty_jump_destination();
    emit(OpCode::JUMP, jump_to_end);
    current_function()->patch_jump_destination(jump_to_else, current_program().size());
    emit(OpCode::POP);
    if (stmt.else_expr) {
        current_scope().pop_temporary(); // current result does not exist
        visit_expr(*stmt.else_expr); // will mark new
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
                   [this](const BlockExpr &expr) { block(expr); },
                   [this](const IfExpr &expr) { if_expression(expr); },
                   [this](const LoopExpr &expr) { loop_expression(expr); },
                   [this](const BreakExpr &expr) { break_expr(expr); },
                   [this](const ContinueExpr &expr) { continue_expr(expr); },
                   [this](const WhileExpr &expr) { while_expr(expr); },
                   [this](const ForExpr &expr) { for_expr(expr); },
                   [this](const ReturnExpr &expr) { retrun_expression(expr); },
                   [this](const ThisExpr &expr) { this_expr(); }
               }, expression);
}

void Compiler::literal(const LiteralExpr &expr) {
    current_scope().mark_temporary();
    int index = current_function()->add_constant(expr.literal);
    emit(OpCode::CONSTANT);
    emit(index); // handle overflow!!!
}

void Compiler::string_literal(const StringLiteral &expr) {
    current_scope().mark_temporary();
    std::string s = expr.string.substr(1, expr.string.size() - 2);
    int index = current_function()->add_constant(s); // memory!
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
        current_scope().pop_temporary();
        return;
    }

    visit_expr(*expr.left);
    // we need handle logical expressions before we execute right side as they can short circut
    if (expr.op == Token::Type::AND_AND || expr.op == Token::Type::BAR_BAR) {
        logical(expr);
        current_scope().pop_temporary();
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
    current_scope().pop_temporary();
}

// TODO: total mess and loads of overlap with Compiler::resolve_variable
void Compiler::update_lvalue(const Expr &lvalue) {
    if (std::holds_alternative<VariableExpr>(lvalue)) {
        std::string name = std::get<VariableExpr>(lvalue).identifier.get_lexeme(source);
        auto resolution = current_context().resolve_variable(name);
        if (std::holds_alternative<Context::LocalResolution>(resolution)) {
            emit(OpCode::SET, std::get<Context::LocalResolution>(resolution).slot); // todo: handle overflow
        } else if (std::holds_alternative<Context::FieldResolution>(resolution)) {
            // TODO: this doesn't work with nested classes i think?
            emit(OpCode::THIS);
            emit(OpCode::SET_PROPERTY, current_function()->add_constant(name));
        } else {
            auto up_resolution = resolve_upvalue(name);
            if (std::holds_alternative<Context::LocalResolution>(up_resolution)) {
                emit(OpCode::SET_UPVALUE, std::get<Context::LocalResolution>(up_resolution).slot);
                // todo: handle overflow
            } else if (std::holds_alternative<Context::FieldResolution>(up_resolution)) {
                // TODO: this doesn't work with nested classes i think?
                emit(OpCode::THIS);
                emit(OpCode::SET_PROPERTY, current_function()->add_constant(name));
            }
        }
    } else if (std::holds_alternative<GetPropertyExpr>(lvalue)) {
        const auto &property_expr = std::get<GetPropertyExpr>(lvalue);
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
    current_scope().pop_temporary(expr.arguments.size());
    emit(OpCode::CALL, expr.arguments.size()); // TODO: check
}

void Compiler::get_property(const GetPropertyExpr &expr) {
    visit_expr(*expr.left);
    std::string name = expr.property.get_lexeme(source);
    int constant = current_function()->add_constant(name);
    emit(OpCode::GET_PROPERTY, constant);
}

void Compiler::super(const SuperExpr &expr) {
    int constant = current_function()->add_constant(expr.method.get_lexeme(source));
    // or just resolve this in vm?
    emit(OpCode::THIS);
    //resolve_variable("super");
    emit(OpCode::GET_SUPER, constant);
    current_scope().mark_temporary();
}
