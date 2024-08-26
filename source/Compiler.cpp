#include "Compiler.h"

#include <cassert>
#include <ranges>

#include "Analyzer.h"
#include "debug.h"
#include "api/enhance_messages.h"
#include "base/overloaded.h"
#include "shared/SharedContext.h"

bool Compiler::compile() {
    ast = parser.parse();
    auto messages = parser.get_messages();
    if (!messages.empty()) {
        auto enchanced = bite::enchance_messages(messages);
        for (auto& msg : enchanced) {
            shared_context->logger.log(bite::Logger::Level::info, msg);
        }
    }
    if (parser.has_errors()) {
        shared_context->diagnostics.print(std::cout, true);
        shared_context->logger.log(
            bite::Logger::Level::error,
            "compiliation aborted because of above errors",
            nullptr
        ); // TODO: workaround
        return false;
    }
    analyzer.analyze(ast);
    if (analyzer.has_errors()) {
        shared_context->diagnostics.print(std::cout, true);
        shared_context->logger.log(
            bite::Logger::Level::error,
            "compiliation aborted because of above errors",
            nullptr
        ); // TODO: workaround
        return false;
    }
    for (auto& stmt : ast.stmts) {
        visit(*stmt);
    }
    // default return at main
    emit_default_return();
    return true;
}

Function& Compiler::get_main() {
    return main;
}

const std::vector<Function*>& Compiler::get_functions() {
    return functions;
}

const std::vector<std::string>& Compiler::get_natives() {
    return natives;
}

void Compiler::this_expr(const ThisExpr&) {
    // safety: check if used in class method context
    emit(OpCode::THIS);
}

void Compiler::start_context(Function* function, FunctionType type) {
    context_stack.emplace_back(function, type);
    current_context().on_stack = function->get_arity() + 1; // plus one for reserved receiver slot!
}

//#define COMPILER_PRINT_BYTECODE

void Compiler::end_context() {
    #ifdef COMPILER_PRINT_BYTECODE
    Disassembler disassembler(*current_function());
    disassembler.disassemble(current_function()->to_string());
    #endif

    context_stack.pop_back();
}

Compiler::Context& Compiler::current_context() {
    return context_stack.back();
}


Function* Compiler::current_function() {
    return current_context().function;
}

Program& Compiler::current_program() {
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
    current_context().on_stack++; // shouldn't matter?
    emit(OpCode::RETURN);
}

void Compiler::define_variable(const DeclarationInfo& info) {
    // TODO: refactor!
    if (std::holds_alternative<LocalDeclarationInfo>(info)) {
        auto local = std::get<LocalDeclarationInfo>(info);
        bool is_captured = local.is_captured;
        current_context().slots[local.idx] = { current_context().on_stack - 1, is_captured };
        if (is_captured) {
            current_context().open_upvalues_slots.insert(current_context().on_stack - 1);
        }
    } else if (std::holds_alternative<GlobalDeclarationInfo>(info)) {
        auto name_constant = current_function()->add_constant(*std::get<GlobalDeclarationInfo>(info).name);
        // TODO: refactor handling system
        emit(OpCode::SET_GLOBAL, name_constant);
    } else {
        // panic!
        std::unreachable();
    }
}

int64_t Compiler::synthetic_variable() {
    return current_context().on_stack - 1;
}

void Compiler::variable_declaration(const VariableDeclaration& expr) {
    if (expr.value) {
        visit(**expr.value);
    }
    define_variable(expr.info);
}

void Compiler::function_declaration(const FunctionDeclaration& stmt) {
    function(stmt, FunctionType::FUNCTION);
    current_context().on_stack++;
    define_variable(stmt.info);
}

void Compiler::native_declaration(const NativeDeclaration& stmt) {
    std::string name = *stmt.name.string;
    natives.push_back(name);
    int idx = current_function()->add_constant(name);
    emit(OpCode::GET_NATIVE, idx);
    current_context().on_stack++;
    // TODO: better way to get those bindings
    define_variable(stmt.info);
}

void Compiler::block_expr(const BlockExpr& expr) {
    int break_idx = current_function()->add_empty_jump_destination();
    ExpressionScope scope {
            expr.label
                ? ExpressionScope(LabeledBlockScope { .label = expr.label->string, .break_idx = break_idx })
                : ExpressionScope(BlockScope())
        };
    with_expression_scope(
        std::move(scope),
        [&expr, this](const ExpressionScope& expr_scope) {
            for (const auto& stmt : expr.stmts) {
                visit(*stmt);
            }
            if (expr.expr) {
                visit(**expr.expr);
                emit(OpCode::SET, get_return_slot(expr_scope));
                emit(OpCode::POP);
                current_context().on_stack--;
            }
        }
    );
    current_function()->patch_jump_destination(break_idx, current_program().size());
}

void Compiler::loop_expr(const LoopExpr& expr) {
    std::optional<StringTable::Handle> label = expr.label ? expr.label->string : std::optional<StringTable::Handle> {};
    int continue_idx = current_function()->add_empty_jump_destination();
    int break_idx = current_function()->add_empty_jump_destination();

    with_expression_scope(
        LoopScope { .label = label, .break_idx = break_idx, .continue_idx = continue_idx },
        [&expr, this, continue_idx](const ExpressionScope&) {
            current_function()->patch_jump_destination(continue_idx, current_program().size());
            // TODO: remove this expression?
            with_expression_scope(
                BlockScope(),
                [this, &expr](const ExpressionScope&) {
                    for (const auto& stmt : expr.body->stmts) {
                        visit(*stmt);
                    }
                    if (expr.body->expr) {
                        visit(**expr.body->expr);
                    }
                }
            );
        }
    );
    emit(OpCode::JUMP, continue_idx);
    current_function()->patch_jump_destination(break_idx, current_program().size());;
}

void Compiler::pop_out_of_scopes(int64_t depth) {
    for (int64_t i = 0; i < depth; ++i) {
        ExpressionScope& scope = current_context().expression_scopes[current_context().expression_scopes.size() - i -
            1];
        // every scope in bite is an expression which should produce a value
        // so we actually leave last value on the stack
        // every time we begin scope we have to remeber that (TODO: maybe this system could be rewritten more clearly)
        bool leave_last = i == depth - 1;
        std::int64_t on_stack_before = get_on_stack_before(scope);
        // Note we actually produce one value
        for (std::int64_t j = 0; j < current_context().on_stack - on_stack_before - leave_last; ++j) {
            // TODO: bug waiting to happen!
            if (current_context().open_upvalues_slots.contains(current_context().on_stack - j - 1)) {
                emit(OpCode::CLOSE_UPVALUE);
            } else {
                emit(OpCode::POP); // TODO: upvalues!
            }
        }
        current_context().on_stack = on_stack_before + leave_last;
    }
}

void Compiler::while_expr(const WhileExpr& expr) {
    // TODO: potential desugaring:
    // auto negated_cond = ast.make_node<UnaryExpr>(ast.make_copy(expr->condition), Token::Type::BANG);
    // auto break_stmt = ast.make_node<ExprStmt>(
    //     ast.make_node<IfExpr>(std::move(negated_cond), ast.make_node<ExprStmt>(ast.make_node<BreakExpr>()), std::optional<Expr>())
    // );
    // auto desugared_while = ast.make_node<LoopExpr>(
    //     ast.make_node<BlockExpr>(std::vector<Stmt> { std::move(break_stmt) }, std::move(expr->body)),
    //     expr->label
    // );
    // loop_expression(desugared_while);
    std::optional<StringTable::Handle> label = expr.label ? expr.label->string : std::optional<StringTable::Handle> {};
    int continue_idx = current_function()->add_empty_jump_destination();
    int break_idx = current_function()->add_empty_jump_destination();
    int end_idx = current_function()->add_empty_jump_destination();
    with_expression_scope(
        LoopScope { .label = label, .break_idx = break_idx, .continue_idx = continue_idx },
        [&expr, this, continue_idx, end_idx](const ExpressionScope&) {
            current_function()->patch_jump_destination(continue_idx, current_program().size());

            visit(*expr.condition);
            emit(OpCode::JUMP_IF_FALSE, end_idx);
            // pop condition evaluation
            emit(OpCode::POP);
            current_context().on_stack--;

            // TODO: remove this expression?
            with_expression_scope(
                BlockScope(),
                [this, &expr](const ExpressionScope&) {
                    for (const auto& stmt : expr.body->stmts) {
                        visit(*stmt);
                    }
                    if (expr.body->expr) {
                        visit(**expr.body->expr);
                    }
                }
            );
        }
    );
    emit(OpCode::JUMP, continue_idx);
    current_function()->patch_jump_destination(end_idx, current_program().size());
    emit(OpCode::POP); // pop evalutaion condition result
    current_function()->patch_jump_destination(break_idx, current_program().size());
}

void Compiler::for_expr(const ForExpr& expr) {
    // TODO: desugaring step!
    with_expression_scope(
        BlockScope(),
        [&expr, this](const ExpressionScope& scope) {
            visit(*expr.iterable);
            int iterator_constant = current_function()->add_constant("iterator");
            emit(OpCode::GET_PROPERTY, iterator_constant);
            emit(OpCode::CALL, 0);
            int64_t iterator_slot = synthetic_variable();
            std::optional<StringTable::Handle> label = expr.label
                                                           ? expr.label->string
                                                           : std::optional<StringTable::Handle>();
            int continue_idx = current_function()->add_empty_jump_destination();
            int break_idx = current_function()->add_empty_jump_destination();
            int end_idx = current_function()->add_empty_jump_destination();
            with_expression_scope(
                LoopScope { .label = label, .break_idx = break_idx, .continue_idx = continue_idx },
                [this, continue_idx, &expr, end_idx, iterator_slot](const ExpressionScope&) {
                    current_function()->patch_jump_destination(continue_idx, current_program().size());
                    // begin condition
                    emit(OpCode::GET, iterator_slot);
                    int condition_constant = current_function()->add_constant("has_next");
                    emit(OpCode::GET_PROPERTY, condition_constant);
                    emit(OpCode::CALL, 0);
                    // end condition

                    emit(OpCode::JUMP_IF_FALSE, end_idx);
                    emit(OpCode::POP); // pop evaluation condition result

                    // begin item
                    emit(OpCode::GET, iterator_slot);
                    int item_constant = current_function()->add_constant("next");
                    emit(OpCode::GET_PROPERTY, item_constant);
                    emit(OpCode::CALL, 0);
                    current_context().on_stack++;
                    define_variable(expr.info);
                    // end item

                    with_expression_scope(
                        BlockScope(),
                        [this, &expr](const ExpressionScope&) {
                            for (const auto& stmt : expr.body->stmts) {
                                visit(*stmt);
                            }
                            if (expr.body->expr) {
                                visit(**expr.body->expr);
                            }
                        }
                    );
                }
            );
            emit(OpCode::JUMP, continue_idx);
            current_function()->patch_jump_destination(end_idx, current_program().size());
            emit(OpCode::POP); // pop evalutaion condition result
            current_function()->patch_jump_destination(break_idx, current_program().size());
            emit(OpCode::SET, get_return_slot(scope));
        }
    );
}


void Compiler::break_expr(const BreakExpr& expr) {
    // refactor!
    int64_t pop_out_depth = 0;
    for (auto& scope : current_context().expression_scopes | std::views::reverse) {
        if (expr.label) {
            if ((std::holds_alternative<LabeledBlockScope>(scope) && std::get<LabeledBlockScope>(scope).label == expr.
                label->string) || (std::holds_alternative<LoopScope>(scope) && std::get<LoopScope>(scope).label &&
                std::get<LoopScope>(scope).label == expr.label->string)) {
                break;
            }
        } else if (std::holds_alternative<LoopScope>(scope)) {
            break;
        }
        ++pop_out_depth;
    }
    // assert we found scope to pop out!
    ExpressionScope& scope = current_context().expression_scopes[current_context().expression_scopes.size() -
        pop_out_depth - 1];
    if (expr.expr) {
        visit(**expr.expr);
        emit(OpCode::SET, get_return_slot(scope));
        emit(OpCode::POP);
        current_context().on_stack--;
    }
    pop_out_of_scopes(pop_out_depth + 1);
    // refactor?
    // assert current scope has break idx!
    emit(
        OpCode::JUMP,
        std::holds_alternative<LoopScope>(scope)
            ? std::get<LoopScope>(scope).break_idx
            : std::get<LabeledBlockScope>(scope).break_idx
    );
}

void Compiler::continue_expr(const ContinueExpr& expr) {
    // overlap with break expr maybe refactor?
    int64_t pop_out_depth = 0;
    for (auto& scope : current_context().expression_scopes | std::views::reverse) {
        if (std::holds_alternative<LoopScope>(scope)) {
            if (expr.label) {
                if (std::get<LoopScope>(scope).label && std::get<LoopScope>(scope).label == expr.label->string) {
                    break;
                }
            } else {
                break;
            }
        }
        ++pop_out_depth;
    }
    // assert we found scope to pop out!
    ExpressionScope& scope = current_context().expression_scopes[current_context().expression_scopes.size() -
        pop_out_depth - 1];
    pop_out_of_scopes(pop_out_depth + 1);
    // refactor?
    // assert current scope has continue idx!
    emit(OpCode::JUMP, std::get<LoopScope>(scope).continue_idx);
}


void Compiler::function(const FunctionDeclaration& stmt, FunctionType type) {
    // TODO: integrate into new strings
    auto function_name = *stmt.name.string;
    auto* function = new Function(function_name, stmt.params.size());
    functions.push_back(function);

    with_context(
        function,
        type,
        [&stmt, this] {
            current_function()->set_upvalue_count(stmt.enviroment.upvalues.size());
            visit(**stmt.body); // TODO: assert has body?
            emit_default_return();
        }
    );

    int constant = current_function()->add_constant(function);
    emit(OpCode::CLOSURE, constant);
    for (const UpValue& upvalue : stmt.enviroment.upvalues) {
        emit(upvalue.local);
        if (upvalue.local) {
            emit(current_context().slots[upvalue.idx].index);
        } else {
            emit(upvalue.idx);
        }
    }
}

void Compiler::constructor(const Constructor& stmt, const std::vector<Field>& fields, bool has_superclass) {
    // refactor: tons of overlap with function generator
    auto* function = new Function("constructor", stmt.function->params.size());
    functions.push_back(function);

    with_context(
        function,
        FunctionType::CONSTRUCTOR,
        [&fields, this, &stmt, has_superclass] {
            if (stmt.has_super) {
                for (auto& expr : stmt.super_arguments) {
                    visit(*expr);
                }
                auto super_arguments_size = std::ranges::distance(stmt.super_arguments);
                // maybe better way to do this instead of this superinstruction?
                emit(OpCode::CALL_SUPER_CONSTRUCTOR, super_arguments_size);
                emit(OpCode::POP); // discard constructor response
            } else if (has_superclass && stmt.super_arguments.empty()) {
                // default superclass construct
                emit(OpCode::CALL_SUPER_CONSTRUCTOR, 0);
                emit(OpCode::POP); // discard constructor response
            }

            // default initialize
            for (const auto& field : fields) {
                // possibly desugar
                if (field.attributes[ClassAttributes::ABSTRACT]) {
                    continue;
                }
                visit(**field.variable->value);
                emit(OpCode::THIS);
                int property_name = current_function()->add_constant(*field.variable->name.string);
                emit(OpCode::SET_PROPERTY, property_name);
                emit(OpCode::POP); // pop value;
            }
            if (stmt.function->body) {
                visit(**stmt.function->body);
            }
            current_function()->set_upvalue_count(stmt.function->enviroment.upvalues.size());
            emit_default_return();
        }
    );

    int constant = current_function()->add_constant(function);
    emit(OpCode::CLOSURE, constant);
    for (const UpValue& upvalue : stmt.function->enviroment.upvalues) {
        emit(upvalue.local);
        if (upvalue.local) {
            emit(current_context().slots[upvalue.idx].index);
        } else {
            emit(upvalue.idx);
        }
    }
}

void Compiler::object_expr(const ObjectExpr& expr) {
    // TODO: refactor! tons of overlap with class
    std::string name = "object"; // todo: check!
    uint8_t name_constant = current_function()->add_constant(name);
    emit(OpCode::NIL);
    emit(OpCode::CLASS, name_constant);

    if (expr.super_class) {
        emit_get_variable(expr.superclass_binding);
        emit(OpCode::INHERIT);
    }

    // TODO: overlap with trait declaration
    for (const auto& using_stmt : expr.body.using_statements) {
        for (const auto& item : using_stmt.items) {
            for (auto& [original_name, field_name, attr] : item.declarations) {
                int field_name_constant = current_function()->add_constant(*original_name);
                // TODO: performance
                // TODO: shouldn't this be in trait declaration as well
                // TODO: refactor
                if (attr[ClassAttributes::GETTER]) {
                    emit_get_variable(item.binding);
                    emit(OpCode::GET_TRAIT, field_name_constant);
                    bitflags<ClassAttributes> hack;
                    hack += ClassAttributes::GETTER;
                    emit(hack.to_ullong());
                    int aliased_name_constant = current_function()->add_constant(*field_name);
                    emit(OpCode::METHOD, aliased_name_constant);
                    emit(hack.to_ullong());
                }
                if (attr[ClassAttributes::SETTER]) {
                    emit_get_variable(item.binding);
                    emit(OpCode::GET_TRAIT, field_name_constant);
                    bitflags<ClassAttributes> hack;
                    hack += ClassAttributes::SETTER;
                    emit(hack.to_ullong());
                    int aliased_name_constant = current_function()->add_constant(*field_name);
                    emit(OpCode::METHOD, aliased_name_constant);
                    emit(hack.to_ullong());
                }
                if (!attr[ClassAttributes::GETTER] && !attr[ClassAttributes::SETTER]) {
                    emit_get_variable(item.binding);
                    emit(OpCode::GET_TRAIT, field_name_constant);
                    emit(0);
                    int aliased_name_constant = current_function()->add_constant(*field_name);
                    emit(OpCode::METHOD, aliased_name_constant);
                    emit(attr.to_ullong());
                }
            }
        }
    }

    for (const auto& field : expr.body.fields) {
        std::string field_name = *field.variable->name.string;
        int field_constant = current_function()->add_constant(field_name);
        emit(OpCode::FIELD, field_constant);
        emit(field.attributes.to_ullong());
    }

    for (auto& method : expr.body.methods) {
        std::string method_name = *method.function->name.string;
        if (!method.attributes[ClassAttributes::ABSTRACT]) {
            function(*method.function, FunctionType::METHOD);
        }
        int idx = current_function()->add_constant(method_name);
        emit(OpCode::METHOD, idx);
        emit(method.attributes.to_ullong()); // check size?
        current_context().on_stack--;
    }

    if (expr.body.constructor) { // TODO: always must have constructor!
        bool has_super_class = static_cast<bool>(expr.super_class);
        constructor(*expr.body.constructor, expr.body.fields, has_super_class);
    }

    emit(OpCode::CONSTRUCTOR);
    emit(OpCode::CALL, 0);
}

void Compiler::object_declaration(const ObjectDeclaration& stmt) {
    visit(*stmt.object);
    define_variable(stmt.info);
}

void Compiler::trait_declaration(const TraitDeclaration& stmt) {
    std::string name = *stmt.name.string;
    uint8_t name_constanst = current_function()->add_constant(name);
    emit(OpCode::TRAIT, name_constanst);
    define_variable(stmt.info);

    for (const auto& using_stmt : stmt.using_stmts) {
        for (const auto& item : using_stmt.items) {
            for (const auto& [original_name, field_name, attr] : item.declarations) {
                int field_name_constant = current_function()->add_constant(*original_name);
                emit_get_variable(item.binding);
                emit(OpCode::GET_TRAIT, field_name_constant);
                emit(0); // TODO HACK
                int aliased_name_constant = current_function()->add_constant(*field_name);
                emit(OpCode::TRAIT_METHOD, aliased_name_constant);
                emit(attr.to_ullong());
            }
        }
    }

    for (const auto& method : stmt.methods) {
        if (!method.attributes[ClassAttributes::ABSTRACT]) {
            function(*method.function, FunctionType::METHOD);
        }
        int method_name_constant = current_function()->add_constant(*method.function->name.string);
        emit(OpCode::TRAIT_METHOD, method_name_constant);
        emit(method.attributes.to_ullong()); // check!
    }

    for (const auto& [name, info] : stmt.enviroment.requirements) {
        // pass requirements down
        int constant_idx = current_function()->add_constant(*name);
        emit(OpCode::TRAIT_METHOD, constant_idx);
        // TODO: pass attributes also!
        auto attr(info.attributes);
        attr += ClassAttributes::ABSTRACT;
        emit(attr.to_ullong());
    }
}

void Compiler::class_declaration(const ClassDeclaration& stmt) {
    // TODO: refactor!
    std::string name = *stmt.name.string;
    uint8_t name_constant = current_function()->add_constant(name);
    if (stmt.body.class_object) {
        object_expr(**stmt.body.class_object);
    } else {
        emit(OpCode::NIL);
        current_context().on_stack++;
    }
    if (stmt.is_abstract) {
        emit(OpCode::ABSTRACT_CLASS, name_constant);
    } else {
        emit(OpCode::CLASS, name_constant);
    }
    define_variable(stmt.info);

    if (stmt.super_class) {
        emit_get_variable(stmt.superclass_binding);
        emit(OpCode::INHERIT);
    }

    // TODO: overlap with trait declaration
    for (const auto& using_stmt : stmt.body.using_statements) {
        for (const auto& item : using_stmt.items) {
            for (auto& [original_name, field_name, attr] : item.declarations) {
                int field_name_constant = current_function()->add_constant(*original_name);
                // TODO: performance
                // TODO: shouldn't this be in trait declaration as well
                // TODO: refactor
                if (attr[ClassAttributes::GETTER]) {
                    emit_get_variable(item.binding);
                    emit(OpCode::GET_TRAIT, field_name_constant);
                    bitflags<ClassAttributes> hack;
                    hack += ClassAttributes::GETTER;
                    emit(hack.to_ullong());
                    int aliased_name_constant = current_function()->add_constant(*field_name);
                    emit(OpCode::METHOD, aliased_name_constant);
                    emit(hack.to_ullong());
                }
                if (attr[ClassAttributes::SETTER]) {
                    emit_get_variable(item.binding);
                    emit(OpCode::GET_TRAIT, field_name_constant);
                    bitflags<ClassAttributes> hack;
                    hack += ClassAttributes::SETTER;
                    emit(hack.to_ullong());
                    int aliased_name_constant = current_function()->add_constant(*field_name);
                    emit(OpCode::METHOD, aliased_name_constant);
                    emit(hack.to_ullong());
                }
                if (!attr[ClassAttributes::GETTER] && !attr[ClassAttributes::SETTER]) {
                    emit_get_variable(item.binding);
                    emit(OpCode::GET_TRAIT, field_name_constant);
                    emit(0);
                    int aliased_name_constant = current_function()->add_constant(*field_name);
                    emit(OpCode::METHOD, aliased_name_constant);
                    emit(attr.to_ullong());
                }
            }
        }
    }

    for (const auto& field : stmt.body.fields) {
        std::string field_name = *field.variable->name.string;
        int field_constant = current_function()->add_constant(field_name);
        emit(OpCode::FIELD, field_constant);
        emit(field.attributes.to_ullong());
    }

    for (auto& method : stmt.body.methods) {
        std::string method_name = *method.function->name.string;
        if (!method.attributes[ClassAttributes::ABSTRACT]) {
            function(*method.function, FunctionType::METHOD);
        }
        int idx = current_function()->add_constant(method_name);
        emit(OpCode::METHOD, idx);
        emit(method.attributes.to_ullong()); // check size?
    }

    if (stmt.body.constructor) { // TODO: always must have constructor!
        bool has_super_class = static_cast<bool>(stmt.super_class);
        constructor(*stmt.body.constructor, stmt.body.fields, has_super_class);
    }

    emit(OpCode::CONSTRUCTOR);
}

void Compiler::expr_stmt(const ExprStmt& stmt) {
    visit(*stmt.value);
    emit(OpCode::POP);
    current_context().on_stack--;
}

void Compiler::return_expr(const ReturnExpr& stmt) {
    if (stmt.value) {
        visit(**stmt.value);
    } else {
        emit(OpCode::NIL);
        current_context().on_stack++;
    }
    emit(OpCode::RETURN);
    // Expression must return value
    emit(OpCode::NIL);
    current_context().on_stack++;
}

void Compiler::if_expr(const IfExpr& stmt) {
    visit(*stmt.condition);
    // TODO: better control flow constructs this is kinda confusing
    int jump_to_else = current_function()->add_empty_jump_destination();
    emit(OpCode::JUMP_IF_FALSE, jump_to_else);
    // maybe combine these into utility some bytecode writer which tracks stack
    emit(OpCode::POP);
    current_context().on_stack--;
    visit(*stmt.then_expr);
    auto jump_to_end = current_function()->add_empty_jump_destination();
    emit(OpCode::JUMP, jump_to_end);
    current_function()->patch_jump_destination(jump_to_else, current_program().size());
    emit(OpCode::POP); // confusing thing is we don't need to pop this off our stack counter (proof left as a exercise)
    if (stmt.else_expr) {
        current_context().on_stack--; // current result does not exist!
        visit(**stmt.else_expr);
    } else {
        emit(OpCode::NIL); // default return value!
    }
    current_function()->patch_jump_destination(jump_to_end, current_program().size());
}

void Compiler::literal_expr(const LiteralExpr& expr) {
    int index = current_function()->add_constant(expr.value);
    emit(OpCode::CONSTANT, index);
    current_context().on_stack++;
}

void Compiler::string_expr(const StringExpr& expr) {
    int index = current_function()->add_constant(expr.string);
    emit(OpCode::CONSTANT, index); // handle overflow!!!
    current_context().on_stack++;
}

void Compiler::unary_expr(const UnaryExpr& expr) {
    visit(*expr.expr);
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

void Compiler::binary_expr(const BinaryExpr& expr) {
    // we don't need to actually visit lhs for plain assigment
    if (expr.op == Token::Type::EQUAL) {
        visit(*expr.right);
        // TODO: refactor?
        if (expr.left->is_get_property_expr()) {
            visit(*expr.left->as_get_property_expr()->left);
        }
        emit_set_variable(expr.binding);
        current_context().on_stack--;
        return;
    }
    visit(*expr.left);
    // we need handle logical expressions before we execute right side as they can short circut
    if (expr.op == Token::Type::AND_AND || expr.op == Token::Type::BAR_BAR) {
        logical_expr(expr);
        current_context().on_stack--;
        return;
    }

    visit(*expr.right);

    // TODO: refactor!!
    if (expr.left->is_get_property_expr() && (expr.op == Token::Type::EQUAL || expr.op == Token::Type::PLUS_EQUAL ||
        expr.op == Token::Type::MINUS_EQUAL || expr.op == Token::Type::STAR_EQUAL || expr.op == Token::Type::SLASH_EQUAL
        || expr.op == Token::Type::SLASH_SLASH_EQUAL || expr.op == Token::Type::AND_EQUAL || expr.op ==
        Token::Type::CARET_EQUAL || expr.op == Token::Type::BAR_EQUAL)) {
        visit(*expr.left->as_get_property_expr());
    }
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
        case Token::Type::PLUS_EQUAL: emit(OpCode::ADD);
            emit_set_variable(expr.binding);
            break;
        case Token::Type::MINUS_EQUAL: emit(OpCode::SUBTRACT);
            emit_set_variable(expr.binding);
            break;
        case Token::Type::STAR_EQUAL: emit(OpCode::MULTIPLY);
            emit_set_variable(expr.binding);
            break;
        case Token::Type::SLASH_EQUAL: emit(OpCode::DIVIDE);
            emit_set_variable(expr.binding);
            break;
        case Token::Type::SLASH_SLASH_EQUAL: emit(OpCode::FLOOR_DIVISON);
            emit_set_variable(expr.binding);
            break;
        case Token::Type::PERCENT_EQUAL: emit(OpCode::MODULO);
            emit_set_variable(expr.binding);
            break;
        case Token::Type::LESS_LESS_EQUAL: emit(OpCode::LEFT_SHIFT);
            emit_set_variable(expr.binding);
            break;
        case Token::Type::GREATER_GREATER_EQUAL: emit(OpCode::RIGHT_SHIFT);
            emit_set_variable(expr.binding);
            break;
        case Token::Type::AND_EQUAL: emit(OpCode::BITWISE_AND);
            emit_set_variable(expr.binding);
            break;
        case Token::Type::CARET_EQUAL: emit(OpCode::BITWISE_XOR);
            emit_set_variable(expr.binding);
            break;
        case Token::Type::BAR_EQUAL: emit(OpCode::BITWISE_OR);
            emit_set_variable(expr.binding);
            break;
        default: std::unreachable(); // panic
    }
    current_context().on_stack--;
}

void Compiler::emit_set_variable(const Binding& binding) {
    std::visit(
        overloaded {
            [this](const LocalBinding& bind) {
                emit(OpCode::SET, current_context().slots[bind.info->idx].index); // assert exists?
            },
            [this](const GlobalBinding& bind) {
                int constant = current_function()->add_constant(*bind.info->name); // TODO: rework constant system
                emit(OpCode::SET_GLOBAL, constant);
            },
            [this](const UpvalueBinding& bind) {
                emit(OpCode::SET_UPVALUE, bind.idx);
            },
            [this](const MemberBinding& bind) {
                emit(OpCode::THIS);
                emit(OpCode::SET_PROPERTY, current_function()->add_constant(*bind.name));
            },
            [this](const ParameterBinding& bind) {
                emit(OpCode::SET, bind.idx + 1); // + 1 for the reserved receiver object
            },
            [this](const ClassObjectBinding& bind) {
                emit_get_variable(*bind.class_binding);
                emit(OpCode::SET_PROPERTY, current_function()->add_constant(*bind.name));
            },
            [this](const PropertyBinding& bind) {
                emit(OpCode::SET_PROPERTY, current_function()->add_constant(*bind.property));
            },
            [this](const SuperBinding& bind) {
                emit(OpCode::THIS);
                emit(OpCode::SET_SUPER, current_function()->add_constant(*bind.property));
            },
            [this](const NoBinding) {
                std::unreachable(); // panic!
            }
        },
        binding
    );
}

void Compiler::emit_get_variable(const Binding& binding) {
    std::visit(
        overloaded {
            [this](const LocalBinding& bind) {
                emit(OpCode::GET, current_context().slots[bind.info->idx].index); // assert exists?
            },
            [this](const GlobalBinding& bind) {
                int constant = current_function()->add_constant(*bind.info->name); // TODO: rework constant system
                emit(OpCode::GET_GLOBAL, constant);
            },
            [this](const UpvalueBinding& bind) {
                emit(OpCode::GET_UPVALUE, bind.idx);
            },
            [this](const MemberBinding& bind) {
                emit(OpCode::THIS);
                emit(OpCode::GET_PROPERTY, current_function()->add_constant(*bind.name));
            },
            [this](const ParameterBinding& bind) {
                emit(OpCode::GET, bind.idx + 1); // + 1 for the reserved receiver object
            },
            [this](const ClassObjectBinding& bind) {
                emit_get_variable(*bind.class_binding);
                emit(OpCode::GET_PROPERTY, current_function()->add_constant(*bind.name));
            },
            [this](const PropertyBinding) {
                // TODO
            },
            [this](const NoBinding) {
                std::unreachable(); // panic!
            },
            [this](const SuperBinding) {
                std::unreachable(); // TODO
            }
        },
        binding
    );
}

void Compiler::variable_expr(const VariableExpr& expr) {
    emit_get_variable(expr.binding);
    current_context().on_stack++;
}

void Compiler::logical_expr(const BinaryExpr& expr) {
    int jump = current_function()->add_empty_jump_destination();
    emit(expr.op == Token::Type::AND_AND ? OpCode::JUMP_IF_FALSE : OpCode::JUMP_IF_TRUE, jump);
    emit(OpCode::POP);
    current_context().on_stack--;
    visit(*expr.right);
    current_function()->patch_jump_destination(jump, current_program().size());
}

void Compiler::call_expr(const CallExpr& expr) {
    visit(*expr.callee);
    for (auto& argument : expr.arguments) {
        visit(*argument);
    }
    auto arguments_size = std::ranges::distance(expr.arguments);
    emit(OpCode::CALL, arguments_size); // TODO: check
    current_context().on_stack -= arguments_size;
}

void Compiler::get_property_expr(const GetPropertyExpr& expr) {
    visit(*expr.left);
    std::string name = *expr.property.string;
    int constant = current_function()->add_constant(name);
    emit(OpCode::GET_PROPERTY, constant);
}

void Compiler::super_expr(const SuperExpr& expr) {
    int constant = current_function()->add_constant(*expr.method.string);
    // or just resolve this in vm?
    emit(OpCode::THIS);
    emit(OpCode::GET_SUPER, constant);
    current_context().on_stack++;
}
