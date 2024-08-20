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
        shared_context->logger.log(
            bite::Logger::Level::error,
            "compiliation aborted because of above errors",
            nullptr
        ); // TODO: workaround
        return false;
    }
    analyzer.analyze(ast);
    if (analyzer.has_errors()) {
        shared_context->logger.log(
            bite::Logger::Level::error,
            "compiliation aborted because of above errors",
            nullptr
        ); // TODO: workaround
        return false;
    }
    for (auto& stmt : ast.statements) {
        visit_stmt(stmt);
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

void Compiler::this_expr() {
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

void Compiler::visit_stmt(const Stmt& statement) {
    std::visit(
        overloaded {
            [this](const AstNode<VarStmt>& stmt) { variable_declaration(stmt); },
            [this](const AstNode<FunctionStmt>& stmt) { function_declaration(stmt); },
            [this](const AstNode<ExprStmt>& stmt) { expr_statement(stmt); },
            [this](const AstNode<ClassStmt>& stmt) { class_declaration(stmt); },
            [this](const AstNode<NativeStmt>& stmt) { native_declaration(stmt); },
            [this](const AstNode<ObjectStmt>& stmt) { object_statement(stmt); },
            [this](const AstNode<TraitStmt>& stmt) { trait_statement(stmt); },
            [this](const AstNode<UsingStmt>&) {},
            [](const AstNode<InvalidStmt>&) {},
        },
        statement
    );
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

void Compiler::variable_declaration(const AstNode<VarStmt>& expr) {
    if (expr->value) {
        visit_expr(*expr->value);
    }
    define_variable(expr->info);
}

void Compiler::function_declaration(const AstNode<FunctionStmt>& stmt) {
    function(stmt, FunctionType::FUNCTION);
    current_context().on_stack++;
    define_variable(stmt->info);
}

void Compiler::native_declaration(const AstNode<NativeStmt>& stmt) {
    std::string name = *stmt->name.string;
    natives.push_back(name);
    int idx = current_function()->add_constant(name);
    emit(OpCode::GET_NATIVE, idx);
    current_context().on_stack++;
    // TODO: better way to get those bindings
    define_variable(stmt->info);
}

void Compiler::block(const AstNode<BlockExpr>& expr) {
    int break_idx = current_function()->add_empty_jump_destination();
    ExpressionScope scope {
            expr->label
                ? ExpressionScope(LabeledBlockScope { .label = expr->label->string, .break_idx = break_idx })
                : ExpressionScope(BlockScope())
        };
    with_expression_scope(
        std::move(scope),
        [&expr, this](const ExpressionScope& expr_scope) {
            for (const auto& stmt : expr->stmts) {
                visit_stmt(stmt);
            }
            if (expr->expr) {
                visit_expr(*expr->expr);
                emit(OpCode::SET, get_return_slot(expr_scope));
                emit(OpCode::POP);
                current_context().on_stack--;
            }
        }
    );
    current_function()->patch_jump_destination(break_idx, current_program().size());
}

void Compiler::loop_expression(const AstNode<LoopExpr>& expr) {
    std::optional<StringTable::Handle> label =
        expr->label ? expr->label->string : std::optional<StringTable::Handle> {};
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
                    for (const auto& stmt : expr->body->stmts) {
                        visit_stmt(stmt);
                    }
                    if (expr->body->expr) {
                        visit_expr(*expr->body->expr);
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

void Compiler::while_expr(const AstNode<WhileExpr>& expr) {
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
    std::optional<StringTable::Handle> label =
        expr->label ? expr->label->string : std::optional<StringTable::Handle> {};
    int continue_idx = current_function()->add_empty_jump_destination();
    int break_idx = current_function()->add_empty_jump_destination();
    int end_idx = current_function()->add_empty_jump_destination();
    with_expression_scope(
        LoopScope { .label = label, .break_idx = break_idx, .continue_idx = continue_idx },
        [&expr, this, continue_idx, end_idx](const ExpressionScope&) {
            current_function()->patch_jump_destination(continue_idx, current_program().size());

            visit_expr(expr->condition);
            emit(OpCode::JUMP_IF_FALSE, end_idx);
            // pop condition evaluation
            emit(OpCode::POP);
            current_context().on_stack--;

            // TODO: remove this expression?
            with_expression_scope(
                BlockScope(),
                [this, &expr](const ExpressionScope&) {
                    for (const auto& stmt : expr->body->stmts) {
                        visit_stmt(stmt);
                    }
                    if (expr->body->expr) {
                        visit_expr(*expr->body->expr);
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

void Compiler::for_expr(const AstNode<ForExpr>& expr) {
    // FIX!
    std::unreachable();
    // TODO: desugaring step!
    // with_expression_scope(
    //     BlockScope(),
    //     [&expr, this](const ExpressionScope&) {
    //         visit_expr(expr->iterable);
    //         int iterator_constant = current_function()->add_constant("iterator");
    //         emit(OpCode::GET_PROPERTY, iterator_constant);
    //         emit(OpCode::CALL, 0);
    //         // define_variable(stmt.inf); // iterator
    //         std::optional<StringTable::Handle> label = expr->label
    //                                                        ? expr->label->string
    //                                                        : std::optional<StringTable::Handle>();
    //         int continue_idx = current_function()->add_empty_jump_destination();
    //         int break_idx = current_function()->add_empty_jump_destination();
    //         int end_idx = current_function()->add_empty_jump_destination();
    //         with_expression_scope(
    //             LoopScope { .label = label, .break_idx = break_idx, .continue_idx = continue_idx },
    //             [this, continue_idx, &expr, end_idx](const ExpressionScope&) {
    //                 current_function()->patch_jump_destination(continue_idx, current_program().size());
    //                 // begin condition
    //                 emit_get_variable(analyzer.bindings[expr.id]);
    //                 int condition_constant = current_function()->add_constant("has_next");
    //                 emit(OpCode::GET_PROPERTY, condition_constant);
    //                 emit(OpCode::CALL, 0);
    //                 // end condition
    //
    //                 emit(OpCode::JUMP_IF_FALSE, end_idx);
    //                 emit(OpCode::POP); // pop evaluation condition result
    //                 current_context().on_stack--;
    //
    //                 // begin item
    //                 // TODO!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //                 define_variable(TODO);
    //                 int item_constant = current_function()->add_constant("next");
    //                 emit(OpCode::GET_PROPERTY, item_constant);
    //                 emit(OpCode::CALL, 0);
    //                 // end item
    //
    //                 with_expression_scope(
    //                     BlockScope(),
    //                     [this, &expr](const ExpressionScope&) {
    //                         for (const auto& stmt : expr->body->stmts) {
    //                             visit_stmt(stmt);
    //                         }
    //                         if (expr->body->expr) {
    //                             visit_expr(*expr->body->expr);
    //                         }
    //                     }
    //                 );
    //             }
    //         );
    //         emit(OpCode::JUMP, continue_idx);
    //         current_function()->patch_jump_destination(end_idx, current_program().size());
    //         emit(OpCode::POP); // pop evalutaion condition result
    //         current_function()->patch_jump_destination(break_idx, current_program().size());
    //     }
    // );

    // // Tons of overlap with while loop maybe abstract this away?
    // // ideally some sort of desugaring step
    // begin_scope(ScopeType::BLOCK);
    // emit(OpCode::NIL);
    // define_variable("$scope_return");
    // // begin define iterator
    // visit_expr(expr.iterable);
    // int iterator_constant = current_function()->add_constant("iterator");
    // emit(OpCode::GET_PROPERTY, iterator_constant);
    // emit(OpCode::CALL, 0);
    // define_variable("$iterator");
    // current_scope().pop_temporary();
    // // end define iterator
    // std::string label = expr.label ? *expr.label->string : "";
    // begin_scope(ScopeType::LOOP, label);
    // emit(OpCode::NIL);
    // define_variable("$scope_return");
    // current_scope().return_slot = std::get<Context::LocalResolution>(
    //     current_context().resolve_variable("$scope_return")
    // ).slot;
    // int continue_idx = current_function()->add_jump_destination(current_program().size());
    // int break_idx = current_function()->add_empty_jump_destination();
    // int end_idx = current_function()->add_empty_jump_destination();
    // current_scope().continue_idx = continue_idx;
    // current_scope().break_idx = break_idx;
    //
    // // begin condition
    // resolve_variable("$iterator");
    // int condition_cosntant = current_function()->add_constant("has_next");
    // emit(OpCode::GET_PROPERTY, condition_cosntant);
    // emit(OpCode::CALL, 0);
    // // end condition
    //
    // emit(OpCode::JUMP_IF_FALSE, end_idx);
    // emit(OpCode::POP); // pop evaluation condition result
    // current_scope().pop_temporary();
    //
    // // begin item
    // resolve_variable("$iterator");
    // int item_constant = current_function()->add_constant("next");
    // emit(OpCode::GET_PROPERTY, item_constant);
    // emit(OpCode::CALL, 0);
    // define_variable(*expr.name.string);
    // current_scope().pop_temporary();
    // // end item
    //
    // visit_expr(expr.body);
    // // ignore block expression result
    // emit(OpCode::POP);
    // current_scope().pop_temporary();
    // pop_out_of_scopes(1);
    // emit(OpCode::JUMP, continue_idx);
    // end_scope();
    // current_function()->patch_jump_destination(end_idx, current_program().size());
    // emit(OpCode::POP); // pop evalutaion condition result
    // current_function()->patch_jump_destination(break_idx, current_program().size());
    // emit(OpCode::SET, *current_scope().get("$scope_return"));
    // emit(OpCode::POP);
    // current_scope().pop_temporary();
    // end_scope();
}


void Compiler::break_expr(const AstNode<BreakExpr>& expr) {
    // refactor!
    int64_t pop_out_depth = 0;
    for (auto& scope : current_context().expression_scopes | std::views::reverse) {
        if (expr->label) {
            if ((std::holds_alternative<LabeledBlockScope>(scope) && std::get<LabeledBlockScope>(scope).label == expr->
                label->string) || (std::holds_alternative<LoopScope>(scope) && std::get<LoopScope>(scope).label &&
                std::get<LoopScope>(scope).label == expr->label->string)) {
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
    if (expr->expr) {
        visit_expr(*expr->expr);
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

void Compiler::continue_expr(const AstNode<ContinueExpr>& expr) {
    // overlap with break expr maybe refactor?
    int64_t pop_out_depth = 0;
    for (auto& scope : current_context().expression_scopes | std::views::reverse) {
        if (std::holds_alternative<LoopScope>(scope)) {
            if (expr->label) {
                if (std::get<LoopScope>(scope).label && std::get<LoopScope>(scope).label == expr->label->string) {
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


void Compiler::function(const AstNode<FunctionStmt>& stmt, FunctionType type) {
    // TODO: integrate into new strings
    auto function_name = *stmt->name.string;
    auto* function = new Function(function_name, stmt->params.size());
    functions.push_back(function);

    with_context(
        function,
        type,
        [&stmt, this] {
            current_function()->set_upvalue_count(stmt->enviroment.upvalues.size());
            visit_expr(*stmt->body); // TODO: assert has body?
            emit_default_return();
        }
    );

    int constant = current_function()->add_constant(function);
    emit(OpCode::CLOSURE, constant);
    for (const UpValue& upvalue : stmt->enviroment.upvalues) {
        emit(upvalue.local);
        if (upvalue.local) {
            emit(current_context().slots[upvalue.idx].index);
        } else {
            emit(upvalue.idx);
        }
    }
}

void Compiler::constructor(
    const Constructor& stmt,
    const std::vector<Field>& fields,
    bool has_superclass,
    int superclass_arguments_count
) {
    // refactor: tons of overlap with function generator
    auto* function = new Function("constructor", stmt.parameters.size());
    functions.push_back(function);

    with_context(
        function,
        FunctionType::CONSTRUCTOR,
        [&fields, this, &stmt] {
            // default initialize
            for (const auto& field : fields) {
                // possibly desugar
                if (field.attributes[ClassAttributes::ABSTRACT]) {
                    continue;
                }
                visit_expr(*field.variable->value);
                emit(OpCode::THIS);
                int property_name = current_function()->add_constant(*field.variable->name.string);
                emit(OpCode::SET_PROPERTY, property_name);
                emit(OpCode::POP); // pop value;
            }
            visit_expr(stmt.body);
            // current_function()->set_upvalue_count(analyzer.function_upvalues[stmt.body.id].size());
            emit_default_return();
        }
    );

    int constant = current_function()->add_constant(function);
    emit(OpCode::CLOSURE, constant);
    // for (const bite::Analyzer::Upvalue& upvalue : analyzer.function_upvalues[stmt.id]) {
    //     emit(upvalue.is_local);
    //     if (upvalue.is_local) {
    //         emit(current_context().slots[upvalue.value].index);
    //     } else {
    //         emit(upvalue.value);
    //     }


    // start_context(function, FunctionType::CONSTRUCTOR);
    // begin_scope(ScopeType::BLOCK); // doesn't context already start a new scope therefor it is reduntant
    // current_scope().define(""); // reserve slot for receiver
    //
    // for (const Token& param : stmt.parameters) {
    //     define_variable(*param.string);
    // }
    //
    // if (stmt.has_super) {
    //     if (!has_superclass) {
    //         assert(false && "No superclass to be constructed");
    //     }
    //     for (const Expr& expr : stmt.super_arguments) {
    //         visit_expr(expr);
    //     }
    //     // maybe better way to do this instead of this superinstruction?
    //     emit(OpCode::CALL_SUPER_CONSTRUCTOR, stmt.super_arguments.size());
    //     emit(OpCode::POP); // discard constructor response
    // } else if (has_superclass) {
    //     if (superclass_arguments_count == 0) {
    //         // default superclass construct
    //         emit(OpCode::CALL_SUPER_CONSTRUCTOR, stmt.super_arguments.size());
    //         emit(OpCode::POP); // discard constructor response
    //     } else {
    //         assert(false && "expected superconstructor call");
    //     }
    // }
    //
    //
    // // default initialize fields
    // for (auto& field : fields) {
    //     // ast builder
    //     if (field.attributes[ClassAttributes::ABSTRACT])
    //         continue;
    //     visit_expr(*field.variable.value);
    //     emit(OpCode::THIS);
    //     int property_name = current_function()->add_constant(*field.variable.name.string);
    //     emit(OpCode::SET_PROPERTY, property_name);
    //     emit(OpCode::POP); // pop value;
    // }
    //
    // visit_expr(stmt.body);
    // emit_default_return();
    // end_scope();
    //
    // function->set_upvalue_count(current_context().upvalues.size());
    // // we need to emit those upvalues in enclosing context (context where function is called)
    // std::vector<Upvalue> function_upvalues = std::move(current_context().upvalues);
    // end_context();
    //
    // int constant = current_function()->add_constant(function);
    // emit(OpCode::CLOSURE, constant);
    //
    // for (const Upvalue& upvalue : function_upvalues) {
    //     emit(upvalue.is_local);
    //     emit(upvalue.index);
    // }
}

void Compiler::default_constructor(const std::vector<Field>& fields, bool has_superclass) {
    // TODO: overlap with function declaration
    // TODO: integrate into new strings
    auto* function = new Function("constructor", 0);
    functions.push_back(function);

    with_context(
        function,
        FunctionType::CONSTRUCTOR,
        [&fields, this] {
            for (const auto& field : fields) {
                // possibly desugar
                if (field.attributes[ClassAttributes::ABSTRACT]) {
                    continue;
                }
                visit_expr(*field.variable->value);
                emit(OpCode::THIS);
                int property_name = current_function()->add_constant(*field.variable->name.string);
                emit(OpCode::SET_PROPERTY, property_name);
                emit(OpCode::POP); // pop value;
            }
            //current_function()->set_upvalue_count(analyzer.function_upvalues[stmt.id].size());
            emit_default_return();
        }
    );

    int constant = current_function()->add_constant(function);
    emit(OpCode::CLOSURE, constant);
    // for (const bite::Analyzer::Upvalue& upvalue : analyzer.function_upvalues[stmt.id]) {
    //     emit(upvalue.is_local);
    //     if (upvalue.is_local) {
    //         emit(current_context().slots[upvalue.value].index);
    //     } else {
    //         emit(upvalue.value);
    //     }
    // }

    // // TODO: ideally in future default constructor has just default parameters!
    // auto* function = new Function("constructor", 0);
    // functions.push_back(function);
    // start_context(function, FunctionType::CONSTRUCTOR);
    // begin_scope(ScopeType::BLOCK); // doesn't context already start a new scope therefor it is reduntant
    // current_scope().define(""); // reserve slot for receiver
    //
    // if (has_superclass) {
    //     // default superclass construct
    //     emit(OpCode::CALL_SUPER_CONSTRUCTOR, 0);
    //     emit(OpCode::POP); // discard constructor response
    // }
    //
    // // default initialize fields
    // for (auto& field : fields) {
    //     // ast builder
    //     if (field.attributes[ClassAttributes::ABSTRACT])
    //         continue;
    //     visit_expr(*field.variable.value);
    //     emit(OpCode::THIS);
    //     int property_name = current_function()->add_constant(*field.variable.name.string);
    //     emit(OpCode::SET_PROPERTY, property_name);
    //     emit(OpCode::POP); // pop value;
    // }
    //
    // emit_default_return();
    // end_scope();
    //
    // function->set_upvalue_count(current_context().upvalues.size());
    // // we need to emit those upvalues in enclosing context (context where function is called)
    // std::vector<Upvalue> function_upvalues = std::move(current_context().upvalues);
    // end_context();
    //
    // int constant = current_function()->add_constant(function);
    // emit(OpCode::CLOSURE, constant);
    //
    // for (const Upvalue& upvalue : function_upvalues) {
    //     emit(upvalue.is_local);
    //     emit(upvalue.index);
    // }
}

// TODO: refactor!!! this whole class parsing part is a total mess!

// refactor: maybe could use Parser::StructrueMembers and type?
void Compiler::class_core(
    int class_slot,
    std::optional<Token> super_class,
    const std::vector<Method>& methods,
    const std::vector<Field>& fields,
    const std::vector<AstNode<UsingStmt>>& using_stmts,
    bool is_abstract
) {
    // if (super_class) {
    //     std::string super_class_name = *super_class->string;
    //     emit(
    //         OpCode::GET,
    //         std::get<Context::LocalResolution>(current_context().resolve_variable(super_class_name)).slot
    //     );
    //     emit(OpCode::GET, class_slot);
    //     emit(OpCode::INHERIT);
    //     emit(OpCode::POP);
    //     assert(current_context().resolved_classes.contains(super_class_name));
    //     auto super_fields = current_context().resolved_classes[super_class_name].fields;
    //     for (auto& field : super_fields) {
    //         if (field.second.attributes[ClassAttributes::PRIVATE])
    //             continue;
    //         current_scope().add_field(field.first, field.second);
    //     }
    // }
    //
    // emit(OpCode::GET, class_slot);
    // current_scope().mark_temporary();
    //
    // // Tracks fields declared in current class body
    // std::unordered_map<std::string, FieldInfo> member_declarations;
    //
    // std::unordered_set<std::string> requirements;
    // // Traits!
    // for (auto& stmt : using_stmts) {
    //     for (auto& item : stmt.items) {
    //         std::string item_name = *item.name.string;
    //
    //         auto trait_fields = current_context().resolved_classes[item_name].fields;
    //         for (auto& [field_name, info] : trait_fields) {
    //             // check if excluded
    //             bool is_excluded = false;
    //             // possibly use some ranges here
    //             for (auto& exclusion : item.exclusions) {
    //                 if (*exclusion.string == field_name) {
    //                     is_excluded = true;
    //                     break;
    //                 }
    //             }
    //             if (is_excluded || info.attributes[ClassAttributes::ABSTRACT]) {
    //                 requirements.insert(field_name);
    //                 // warn about useless exclude?
    //                 continue;
    //             }
    //             // should aliasing methods that are requierments be allowed?
    //             std::string aliased_name = field_name;
    //             for (auto& [before, after] : item.aliases) {
    //                 if (*before.string == field_name) {
    //                     aliased_name = *after.string;
    //                     break;
    //                 }
    //             }
    //             if (current_scope().has_field(aliased_name)) {
    //                 // if field is already declared in current scope then it comes from superclass
    //                 assert(false && "Should overwrite.");
    //             }
    //             // better error here
    //             if (member_declarations.contains(aliased_name)) {
    //                 assert(false && "Member redeclaration is disallowed.");
    //             }
    //             member_declarations[aliased_name] = info;
    //             int field_name_constant = current_function()->add_constant(field_name);
    //             // TODO: performance
    //             if (info.attributes[ClassAttributes::GETTER]) {
    //                 emit(
    //                     OpCode::GET,
    //                     std::get<Context::LocalResolution>(current_context().resolve_variable(item_name)).slot
    //                 );
    //                 emit(OpCode::GET_TRAIT, field_name_constant);
    //                 bitflags<ClassAttributes> hack;
    //                 hack += ClassAttributes::GETTER;
    //                 emit(hack.to_ullong());
    //                 int aliased_name_constant = current_function()->add_constant(aliased_name);
    //                 emit(OpCode::METHOD, aliased_name_constant);
    //                 emit(hack.to_ullong());
    //             }
    //             if (info.attributes[ClassAttributes::SETTER]) {
    //                 emit(
    //                     OpCode::GET,
    //                     std::get<Context::LocalResolution>(current_context().resolve_variable(item_name)).slot
    //                 );
    //                 emit(OpCode::GET_TRAIT, field_name_constant);
    //                 bitflags<ClassAttributes> hack;
    //                 hack += ClassAttributes::SETTER;
    //                 emit(hack.to_ullong());
    //                 int aliased_name_constant = current_function()->add_constant(aliased_name);
    //                 emit(OpCode::METHOD, aliased_name_constant);
    //                 emit(hack.to_ullong());
    //             }
    //             if (!info.attributes[ClassAttributes::GETTER] && !info.attributes[ClassAttributes::SETTER]) {
    //                 emit(
    //                     OpCode::GET,
    //                     std::get<Context::LocalResolution>(current_context().resolve_variable(item_name)).slot
    //                 );
    //                 emit(OpCode::GET_TRAIT, field_name_constant);
    //                 emit(0);
    //                 int aliased_name_constant = current_function()->add_constant(aliased_name);
    //                 emit(OpCode::METHOD, aliased_name_constant);
    //                 emit(info.attributes.to_ullong());
    //             }
    //         }
    //     }
    // }
    //
    // // TODO: disallow init keyword!
    // // TODO: better error handling
    // // TODO: should all be moved to some sort of resolving step
    // // TODO: should check for invalid attributes combinations
    // for (auto& field : fields) {
    //     std::string field_name = *field.variable.name.string;
    //     int idx = current_function()->add_constant(field_name);
    //     emit(OpCode::FIELD, idx);
    //     emit(field.attributes.to_ullong()); // size check?
    //     if (member_declarations.contains(field_name)) {
    //         assert(false && "Member redeclaration is disallowed.");
    //     }
    //     bool should_override = false;
    //     if (current_scope().has_field(field_name)) {
    //         // if field is already declared in current scope then it comes from superclass
    //         should_override = true;
    //     }
    //     if (should_override && !field.attributes[ClassAttributes::OVERRIDE]) {
    //         assert(false && "override attribute expected");
    //     }
    //     if (!should_override && field.attributes[ClassAttributes::OVERRIDE]) {
    //         assert(false && "no member to override.");
    //     }
    //     member_declarations[field_name] = FieldInfo(field.attributes);
    // }
    //
    // // hoist methods
    // for (auto& method : methods) {
    //     // TODO: much overlap with above loop
    //     std::string method_name = *method.function.name.string;
    //     bool already_partially_declared = false;
    //     if (member_declarations.contains(method_name)) {
    //         // special case for getters and setters methods
    //         if (member_declarations[method_name].attributes[ClassAttributes::SETTER] && method.attributes[
    //             ClassAttributes::GETTER]) {
    //             already_partially_declared = true;
    //         } else if (member_declarations[method_name].attributes[ClassAttributes::GETTER] && method.attributes[
    //             ClassAttributes::SETTER]) {
    //             already_partially_declared = true;
    //         } else {
    //             assert(false && "Member redeclaration is disallowed.");
    //         }
    //     }
    //     bool should_override = false;
    //     if (current_scope().has_field(method_name)) {
    //         // if method is already declared in current scope then it comes from superclass
    //         auto field_info = current_scope().get_field_info(method_name);
    //
    //         // special case for getters and setters methods
    //         // TODO: mess
    //         if ((!field_info.attributes[ClassAttributes::GETTER] && method.attributes[ClassAttributes::GETTER]) || (!
    //             field_info.attributes[ClassAttributes::SETTER] && method.attributes[ClassAttributes::SETTER])) {} else {
    //             should_override = true;
    //         }
    //     }
    //     if (should_override && !method.attributes[ClassAttributes::OVERRIDE]) {
    //         assert(false && "override attribute expected");
    //     }
    //     if (!should_override && method.attributes[ClassAttributes::OVERRIDE]) {
    //         assert(false && "no member to override.");
    //     }
    //     // TODO: this does not allow mixing private and public setters and getters!
    //     if (!already_partially_declared) {
    //         member_declarations[method_name] = FieldInfo(method.attributes);
    //     } else {
    //         member_declarations[method_name].attributes += method.attributes[ClassAttributes::GETTER]
    //                                                            ? ClassAttributes::GETTER
    //                                                            : ClassAttributes::SETTER;
    //     }
    // }
    //
    // // check if all abstract classes are overriden
    // if (!is_abstract && super_class) {
    //     for (auto& [name, info] : current_scope().get_fields()) {
    //         if (info.attributes[ClassAttributes::ABSTRACT]) {
    //             if (!member_declarations.contains(name)) {
    //                 assert(false && "Expected abstract override");
    //             }
    //             if (info.attributes[ClassAttributes::GETTER] && !member_declarations[name].attributes[
    //                 ClassAttributes::GETTER]) {
    //                 assert(false && "Expected abstract override for getter");
    //             }
    //             if (info.attributes[ClassAttributes::SETTER] && !member_declarations[name].attributes[
    //                 ClassAttributes::SETTER]) {
    //                 assert(false && "Expected abstract override for setter");
    //             }
    //         }
    //     }
    // }
    //
    // // add declared members to current scope
    // current_scope().get_fields().insert(member_declarations.begin(), member_declarations.end());
    //
    // // Traits validation.
    // // Traits and abstract classes interop?
    // for (auto& requirement : requirements) {
    //     if (!current_scope().has_field(requirement)) {
    //         assert(false && "Failed trait requirement.");
    //     }
    // }
    //
    // for (auto& method : methods) {
    //     std::string method_name = *method.function.name.string;
    //     if (!method.attributes[ClassAttributes::ABSTRACT]) {
    //         function(method.function, FunctionType::METHOD);
    //     }
    //     int idx = current_function()->add_constant(method_name);
    //     emit(OpCode::METHOD, idx);
    //     emit(method.attributes.to_ullong()); // check size?
    //     current_scope().pop_temporary();
    // }
}

// overlaps
void Compiler::object_constructor(
    const std::vector<Field>& fields,
    bool has_superclass,
    const std::vector<Expr>& superclass_arguments
) {
    // auto* function = new Function("constructor", 0);
    // functions.push_back(function);
    // start_context(function, FunctionType::CONSTRUCTOR);
    // begin_scope(ScopeType::BLOCK); // doesn't context already start a new scope therefor it is reduntant
    // current_scope().define(""); // reserve slot for receiver
    //
    // if (has_superclass) {
    //     for (const Expr& expr : superclass_arguments) {
    //         visit_expr(expr);
    //     }
    //     // maybe better way to do this instead of this superinstruction?
    //     emit(OpCode::CALL_SUPER_CONSTRUCTOR, superclass_arguments.size());
    //     emit(OpCode::POP); // discard constructor response
    // }
    //
    // // default initialize fields
    // for (auto& field : fields) {
    //     // ast builder
    //     if (field.attributes[ClassAttributes::ABSTRACT])
    //         continue;
    //     visit_expr(*field.variable.value);
    //     emit(OpCode::THIS);
    //     int property_name = current_function()->add_constant(*field.variable.name.string);
    //     emit(OpCode::SET_PROPERTY, property_name);
    //     emit(OpCode::POP); // pop value;
    // }
    // emit_default_return();
    // end_scope();
    //
    // function->set_upvalue_count(current_context().upvalues.size());
    // // we need to emit those upvalues in enclosing context (context where function is called)
    // std::vector<Upvalue> function_upvalues = std::move(current_context().upvalues);
    // end_context();
    //
    // int constant = current_function()->add_constant(function);
    // emit(OpCode::CLOSURE, constant);
    //
    // for (const Upvalue& upvalue : function_upvalues) {
    //     emit(upvalue.is_local);
    //     emit(upvalue.index);
    // }
}

void Compiler::object_expression(const AstNode<ObjectExpr>& expr) {
    // // TODO: refactor!
    // begin_scope(ScopeType::BLOCK);
    // emit(OpCode::NIL);
    // define_variable("$scope_return");
    // std::string name = "object"; // todo: check!
    // uint8_t name_constant = current_function()->add_constant(name);
    // emit(OpCode::NIL);
    // emit(OpCode::CLASS, name_constant);
    // current_scope().define(name);
    //
    // begin_scope(ScopeType::CLASS, name);
    // // TODO: non-expression scope?
    // emit(OpCode::NIL);
    // define_variable("$scope_return");
    //
    // // TODO: traits in objects.
    // class_core(
    //     std::get<Context::LocalResolution>(current_context().resolve_variable(name)).slot,
    //     expr.super_class,
    //     expr.body.methods,
    //     expr.body.fields,
    //     expr.body.using_statements,
    //     false
    // );
    //
    // object_constructor(expr.body.fields, expr.super_class.has_value(), expr.superclass_arguments);
    // emit(OpCode::CONSTRUCTOR);
    //
    // emit(OpCode::POP);
    // current_scope().pop_temporary();
    // // TODO: non-expression scope?
    // end_scope();
    // emit(OpCode::POP);
    // current_scope().pop_temporary();
    // // constructing object logic goes here!
    // emit(OpCode::CALL, 0);
    // emit(OpCode::SET, std::get<Context::LocalResolution>(current_context().resolve_variable("$scope_return")).slot);
    // end_scope();
}

void Compiler::object_statement(const AstNode<ObjectStmt>& stmt) {
    // visit_expr(stmt.object);
    // current_scope().pop_temporary();
    // define_variable(*stmt.name.string);
}

void Compiler::trait_statement(const AstNode<TraitStmt>& stmt) {
    // std::string name = *stmt.name.string;
    // uint8_t name_constanst = current_function()->add_constant(name);
    //
    // emit(OpCode::TRAIT, name_constanst);
    // current_scope().define(name);
    // // non-expression scopes!
    // begin_scope(ScopeType::CLASS, name);
    // emit(OpCode::NIL);
    // current_scope().define("$scope_exit");
    // // TODO: fields support!
    // resolve_variable(name);
    // std::unordered_set<std::string> requirements;
    // // Traits!
    // for (auto& stmt : stmt.using_stmts) {
    //     for (auto& item : stmt.items) {
    //         std::string item_name = *item.name.string;
    //
    //         auto trait_fields = current_context().resolved_classes[item_name].fields;
    //         for (auto& [field_name, info] : trait_fields) {
    //             // check if excluded
    //             bool is_excluded = false;
    //             // possibly use some ranges here
    //             for (auto& exclusion : item.exclusions) {
    //                 if (*exclusion.string == field_name) {
    //                     is_excluded = true;
    //                     break;
    //                 }
    //             }
    //             if (is_excluded || info.attributes[ClassAttributes::ABSTRACT]) {
    //                 requirements.insert(field_name);
    //                 // warn about useless exclude?
    //                 continue;
    //             }
    //             // should aliasing methods that are requierments be allowed?
    //             std::string aliased_name = field_name;
    //             for (auto& [before, after] : item.aliases) {
    //                 if (*before.string == field_name) {
    //                     aliased_name = *after.string;
    //                     break;
    //                 }
    //             }
    //             // better error here
    //             if (current_scope().has_field(aliased_name)) {
    //                 assert(false && "Member redeclaration is disallowed.");
    //             }
    //             current_scope().add_field(aliased_name, info);
    //             int field_name_constant = current_function()->add_constant(field_name);
    //             emit(
    //                 OpCode::GET,
    //                 std::get<Context::LocalResolution>(current_context().resolve_variable(item_name)).slot
    //             );
    //             emit(OpCode::GET_TRAIT, field_name_constant);
    //             emit(0); // TODO HACK
    //             int aliased_name_constant = current_function()->add_constant(aliased_name);
    //             emit(OpCode::TRAIT_METHOD, aliased_name_constant);
    //             emit(info.attributes.to_ullong());
    //             // TODO: performance
    //         }
    //     }
    // }
    //
    // // mess
    // for (auto& field : stmt.fields) {
    //     std::string field_name = *field.variable.name.string;
    //     if (current_scope().has_field(field_name)) {
    //         assert(false && "field redeclaration is disallowed.");
    //     }
    //     current_scope().add_field(field_name, FieldInfo(field.attributes));
    // }
    //
    // // hoist methods
    // for (auto& method : stmt.methods) {
    //     std::string method_name = *method.function.name.string;
    //     bool partially_defined = false;
    //     // TODO this allowes clashes with real methods???
    //     if (current_scope().has_field(method_name)) {
    //         if ((!current_scope().get_fields()[method_name].attributes[ClassAttributes::SETTER] && method.attributes[
    //             ClassAttributes::SETTER]) || (!current_scope().get_fields()[method_name].attributes[
    //             ClassAttributes::GETTER] && method.attributes[ClassAttributes::GETTER])) {
    //             partially_defined = true;
    //         } else {
    //             assert(false && "member redeclaration is disallowed.");
    //         }
    //     }
    //     if (partially_defined) {
    //         // this does not allowed visibility fields
    //         current_scope().get_fields()[method_name].attributes +=
    //             current_scope().get_fields()[method_name].attributes[ClassAttributes::GETTER]
    //                 ? ClassAttributes::SETTER
    //                 : ClassAttributes::GETTER;
    //     } else {
    //         current_scope().add_field(method_name, FieldInfo(method.attributes));
    //     }
    // }
    //
    // for (auto& method : stmt.methods) {
    //     if (!method.attributes[ClassAttributes::ABSTRACT]) {
    //         function(method.function, FunctionType::METHOD);
    //     }
    //     int method_name_constant = current_function()->add_constant(*method.function.name.string);
    //     emit(OpCode::TRAIT_METHOD, method_name_constant);
    //     emit(method.attributes.to_ullong()); // check!
    // }
    //
    // // shadowing requierments?
    // for (auto& requierment : requirements) {
    //     if (!current_scope().has_field(requierment)) {
    //         // pass requirements down
    //         int constant_idx = current_function()->add_constant(requierment);
    //         emit(OpCode::TRAIT_METHOD, constant_idx);
    //         // TODO: pass attributes also!
    //         bitflags<ClassAttributes> attr;
    //         attr += ClassAttributes::ABSTRACT;
    //         emit(attr.to_ullong());
    //     }
    // }
    //
    //
    // end_scope();
    // emit(OpCode::POP);
    // current_scope().pop_temporary();
}

void Compiler::class_declaration(const AstNode<ClassStmt>& stmt) {
    // // TODO: refactor!
    std::string name = *stmt->name.string;
    uint8_t name_constant = current_function()->add_constant(name);
    //
    if (stmt->body.class_object) {
        std::unreachable(); // TODO
        //visit_expr(*stmt->body.class_object);
    } else {
        emit(OpCode::NIL);
        current_context().on_stack++;
        // current_scope().mark_temporary();
    }
    // if (stmt.is_abstract) {
    //     emit(OpCode::ABSTRACT_CLASS, name_constant);
    // } else {
    //     emit(OpCode::CLASS, name_constant);
    // }
    // current_scope().pop_temporary();
    // current_scope().define(name);

    emit(OpCode::CLASS, name_constant);
    define_variable(stmt->info);

    for (const auto& field : stmt->body.fields) {
        std::string field_name = *field.variable->name.string;
        int field_constant = current_function()->add_constant(field_name);
        emit(OpCode::FIELD, field_constant);
        emit(field.attributes.to_ullong());
    }

    for (auto& method : stmt->body.methods) {
        std::string method_name = *method.function->name.string;
        if (!method.attributes[ClassAttributes::ABSTRACT]) {
            function(method.function, FunctionType::METHOD);
        }
        int idx = current_function()->add_constant(method_name);
        emit(OpCode::METHOD, idx);
        emit(method.attributes.to_ullong()); // check size?
        current_context().on_stack--;
    }

    //
    //
    // begin_scope(ScopeType::CLASS, name);
    // // TODO: non-expression scope?
    // emit(OpCode::NIL);
    // define_variable("$scope_return");
    //
    // class_core(
    //     std::get<Context::LocalResolution>(current_context().resolve_variable(name)).slot,
    //     stmt.super_class,
    //     stmt.body.methods,
    //     stmt.body.fields,
    //     stmt.body.using_statements,
    //     stmt.is_abstract
    // );
    //
    // if (stmt.body.constructor) {
    //     current_scope().constructor_argument_count = stmt.body.constructor->parameters.size();
    //     bool has_super_class = static_cast<bool>(stmt.super_class);
    //     constructor(
    //         *stmt.body.constructor,
    //         stmt.body.fields,
    //         has_super_class,
    //         has_super_class
    //             ? current_context().resolved_classes[*stmt.super_class->string].constructor_argument_count
    //             : 0
    //     );
    // } else {
    //     if (stmt.super_class && current_context().resolved_classes[*stmt.super_class->string].constructor_argument_count
    //         != 0) {
    //         assert(false && "Class must implement constructor because it needs to call superclass constructor");
    //     }
    //     default_constructor(stmt.body.fields, static_cast<bool>(stmt.super_class));
    // }
    if (stmt->body.constructor) {
        // cur().constructor_argument_count = stmt.body.constructor->parameters.size();
        //bool has_super_class = static_cast<bool>(stmt.super_class);
        // TODO: support inheritance
        constructor(*stmt->body.constructor, stmt->body.fields, false, 0);
    } else {
        default_constructor(stmt->body.fields, static_cast<bool>(stmt->super_class));
    }

    emit(OpCode::CONSTRUCTOR);
    //emit(OpCode::POP);

    // current_scope().pop_temporary();
    // // TODO: non-expression scope?
    // end_scope();
    // emit(OpCode::POP);
    // current_scope().pop_temporary();
}

void Compiler::expr_statement(const AstNode<ExprStmt>& stmt) {
    visit_expr(stmt->expr);
    emit(OpCode::POP);
    current_context().on_stack--;
}

void Compiler::return_expression(const AstNode<ReturnExpr>& stmt) {
    if (stmt->value) {
        visit_expr(*stmt->value);
    } else {
        emit(OpCode::NIL);
        current_context().on_stack++;
    }
    emit(OpCode::RETURN);
    // Expression must return value
    emit(OpCode::NIL);
    current_context().on_stack++;
}

void Compiler::if_expression(const AstNode<IfExpr>& stmt) {
    visit_expr(stmt->condition);
    // TODO: better control flow constructs this is kinda confusing
    int jump_to_else = current_function()->add_empty_jump_destination();
    emit(OpCode::JUMP_IF_FALSE, jump_to_else);
    // maybe combine these into utility some bytecode writer which tracks stack
    emit(OpCode::POP);
    current_context().on_stack--;
    visit_expr(stmt->then_expr);
    auto jump_to_end = current_function()->add_empty_jump_destination();
    emit(OpCode::JUMP, jump_to_end);
    current_function()->patch_jump_destination(jump_to_else, current_program().size());
    emit(OpCode::POP); // confusing thing is we don't need to pop this off our stack counter (proof left as a exercise)
    if (stmt->else_expr) {
        current_context().on_stack--; // current result does not exist!
        visit_expr(*stmt->else_expr);
    } else {
        emit(OpCode::NIL); // default return value!
    }
    current_function()->patch_jump_destination(jump_to_end, current_program().size());
}

void Compiler::visit_expr(const Expr& expression) {
    std::visit(
        overloaded {
            [this](const AstNode<LiteralExpr>& expr) { literal(expr); },
            [this](const AstNode<UnaryExpr>& expr) { unary(expr); },
            [this](const AstNode<BinaryExpr>& expr) { binary(expr); },
            [this](const AstNode<StringLiteral>& expr) { string_literal(expr); },
            [this](const AstNode<VariableExpr>& expr) { variable(expr); },
            [this](const AstNode<CallExpr>& expr) { call(expr); },
            [this](const AstNode<GetPropertyExpr>& expr) { get_property(expr); },
            [this](const AstNode<SuperExpr>& expr) { super(expr); },
            [this](const AstNode<BlockExpr>& expr) { block(expr); },
            [this](const AstNode<IfExpr>& expr) { if_expression(expr); },
            [this](const AstNode<LoopExpr>& expr) { loop_expression(expr); },
            [this](const AstNode<BreakExpr>& expr) { break_expr(expr); },
            [this](const AstNode<ContinueExpr>& expr) { continue_expr(expr); },
            [this](const AstNode<WhileExpr>& expr) { while_expr(expr); },
            [this](const AstNode<ForExpr>& expr) { for_expr(expr); },
            [this](const AstNode<ReturnExpr>& expr) { return_expression(expr); },
            [this](const AstNode<ThisExpr>&) { this_expr(); },
            [this](const AstNode<ObjectExpr>& expr) { object_expression(expr); },
            [](const AstNode<InvalidExpr>&) {}
        },
        expression
    );
}

void Compiler::literal(const AstNode<LiteralExpr>& expr) {
    int index = current_function()->add_constant(expr->literal);
    emit(OpCode::CONSTANT, index);
    current_context().on_stack++;
}

void Compiler::string_literal(const AstNode<StringLiteral>& expr) {
    int index = current_function()->add_constant(expr->string);
    emit(OpCode::CONSTANT, index); // handle overflow!!!
    current_context().on_stack++;
}

void Compiler::unary(const AstNode<UnaryExpr>& expr) {
    visit_expr(expr->expr);
    switch (expr->op) {
        case Token::Type::MINUS: emit(OpCode::NEGATE);
            break;
        case Token::Type::BANG: emit(OpCode::NOT);
            break;
        case Token::Type::TILDE: emit(OpCode::BINARY_NOT);
            break;
        default: assert("unreachable");
    }
}

void Compiler::binary(const AstNode<BinaryExpr>& expr) {
    // we don't need to actually visit lhs for plain assigment
    if (expr->op == Token::Type::EQUAL) {
        visit_expr(expr->right);
        // TODO: refactor?
        if (std::holds_alternative<AstNode<GetPropertyExpr>>(expr->left)) {
            visit_expr(std::get<AstNode<GetPropertyExpr>>(expr->left)->left);
        }
        emit_set_variable(expr->binding);
        current_context().on_stack--;
        return;
    }
    visit_expr(expr->left);
    // we need handle logical expressions before we execute right side as they can short circut
    if (expr->op == Token::Type::AND_AND || expr->op == Token::Type::BAR_BAR) {
        logical(expr);
        current_context().on_stack--;
        return;
    }

    visit_expr(expr->right);

    // TODO: refactor!!
    if (std::holds_alternative<AstNode<GetPropertyExpr>>(expr->left) && (expr->op == Token::Type::EQUAL || expr->op ==
        Token::Type::PLUS_EQUAL || expr->op == Token::Type::MINUS_EQUAL || expr->op == Token::Type::STAR_EQUAL || expr->
        op == Token::Type::SLASH_EQUAL || expr->op == Token::Type::SLASH_SLASH_EQUAL || expr->op ==
        Token::Type::AND_EQUAL || expr->op == Token::Type::CARET_EQUAL || expr->op == Token::Type::BAR_EQUAL)) {
        visit_expr(std::get<AstNode<GetPropertyExpr>>(expr->left)->left);
    }
    switch (expr->op) {
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
            emit_set_variable(expr->binding);
            break;
        case Token::Type::MINUS_EQUAL: emit(OpCode::SUBTRACT);
            emit_set_variable(expr->binding);
            break;
        case Token::Type::STAR_EQUAL: emit(OpCode::MULTIPLY);
            emit_set_variable(expr->binding);
            break;
        case Token::Type::SLASH_EQUAL: emit(OpCode::DIVIDE);
            emit_set_variable(expr->binding);
            break;
        case Token::Type::SLASH_SLASH_EQUAL: emit(OpCode::FLOOR_DIVISON);
            emit_set_variable(expr->binding);
            break;
        case Token::Type::PERCENT_EQUAL: emit(OpCode::MODULO);
            emit_set_variable(expr->binding);
            break;
        case Token::Type::LESS_LESS_EQUAL: emit(OpCode::LEFT_SHIFT);
            emit_set_variable(expr->binding);
            break;
        case Token::Type::GREATER_GREATER_EQUAL: emit(OpCode::RIGHT_SHIFT);
            emit_set_variable(expr->binding);
            break;
        case Token::Type::AND_EQUAL: emit(OpCode::BITWISE_AND);
            emit_set_variable(expr->binding);
            break;
        case Token::Type::CARET_EQUAL: emit(OpCode::BITWISE_XOR);
            emit_set_variable(expr->binding);
            break;
        case Token::Type::BAR_EQUAL: emit(OpCode::BITWISE_OR);
            emit_set_variable(expr->binding);
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
            // [this](const ClassObjectBinding) {
            //     // TODO
            // },
            [this](const PropertyBinding& bind) {
                emit(OpCode::SET_PROPERTY, current_function()->add_constant(*bind.property));
            },
            [this](const NoBinding) {
                std::unreachable(); // panic!
            }
        },
        binding
    );

    // if (std::holds_alternative<bite::box<VariableExpr>>(lvalue)) {
    //     std::string name = *std::get<bite::box<VariableExpr>>(lvalue)->identifier.string;
    //     auto resolution = current_context().resolve_variable(name);
    //     if (std::holds_alternative<Context::LocalResolution>(resolution)) {
    //         emit(OpCode::SET, std::get<Context::LocalResolution>(resolution).slot); // todo: handle overflow
    //     } else if (std::holds_alternative<Context::FieldResolution>(resolution)) {
    //         // TODO: this doesn't work with nested classes i think?
    //         emit(OpCode::THIS);
    //         emit(OpCode::SET_PROPERTY, current_function()->add_constant(name));
    //     } else {
    //         auto up_resolution = resolve_upvalue(name);
    //         if (std::holds_alternative<Context::LocalResolution>(up_resolution)) {
    //             emit(OpCode::SET_UPVALUE, std::get<Context::LocalResolution>(up_resolution).slot);
    //             // todo: handle overflow
    //         } else if (std::holds_alternative<Context::FieldResolution>(up_resolution)) {
    //             // TODO: this doesn't work with nested classes i think?
    //             emit(OpCode::THIS);
    //             emit(OpCode::SET_PROPERTY, current_function()->add_constant(name));
    //         }
    //     }
    // } else if (std::holds_alternative<bite::box<GetPropertyExpr>>(lvalue)) {
    //     const auto& property_expr = std::get<bite::box<GetPropertyExpr>>(lvalue);
    //     std::string name = *property_expr->property.string;
    //     int constant = current_function()->add_constant(name);
    //     visit_expr(property_expr->left);
    //     emit(OpCode::SET_PROPERTY, constant);
    // } else if (std::holds_alternative<bite::box<SuperExpr>>(lvalue)) {
    //     const auto& super_expr = std::get<bite::box<SuperExpr>>(lvalue);
    //     int constant = current_function()->add_constant(*super_expr->method.string);
    //     emit(OpCode::THIS);
    //     emit(OpCode::SET_SUPER, constant);
    // } else {
    //     assert("Expected lvalue.");
    // }
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
            // [this](const bite::Analyzer::ClassObjectBinding) {
            //     // TODO
            // }
            [this](const PropertyBinding) {
                // TODO
            },
            [this](const NoBinding) {
                std::unreachable(); // panic!
            }
        },
        binding
    );
}

void Compiler::variable(const AstNode<VariableExpr>& expr) {
    emit_get_variable(expr->binding);
    current_context().on_stack++;
}

void Compiler::logical(const AstNode<BinaryExpr>& expr) {
    int jump = current_function()->add_empty_jump_destination();
    emit(expr->op == Token::Type::AND_AND ? OpCode::JUMP_IF_FALSE : OpCode::JUMP_IF_TRUE, jump);
    emit(OpCode::POP);
    current_context().on_stack--;
    visit_expr(expr->right);
    current_function()->patch_jump_destination(jump, current_program().size());
}

void Compiler::call(const AstNode<CallExpr>& expr) {
    visit_expr(expr->callee);
    for (const Expr& argument : expr->arguments) {
        visit_expr(argument);
    }
    emit(OpCode::CALL, expr->arguments.size()); // TODO: check
    current_context().on_stack -= expr->arguments.size();
}

void Compiler::get_property(const AstNode<GetPropertyExpr>& expr) {
    visit_expr(expr->left);
    std::string name = *expr->property.string;
    int constant = current_function()->add_constant(name);
    emit(OpCode::GET_PROPERTY, constant);
}

void Compiler::super(const AstNode<SuperExpr>& expr) {
    // int constant = current_function()->add_constant(*expr.method.string);
    // // or just resolve this in vm?
    // emit(OpCode::THIS);
    // //resolve_variable("super");
    // emit(OpCode::GET_SUPER, constant);
    // current_scope().mark_temporary();
}
