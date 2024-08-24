#include "Analyzer.h"

#include "base/overloaded.h"

void bite::Analyzer::emit_error_diagnostic(
    const std::string& message,
    const SourceSpan& inline_errror_location,
    const std::string& inline_error_message,
    std::vector<InlineHint>&& additonal_hints
) {
    m_has_errors = true;
    // add error hint to the rest of hints
    additonal_hints.emplace_back(inline_errror_location, inline_error_message, DiagnosticLevel::ERROR);
    context->diagnostics.add(
        { .level = DiagnosticLevel::ERROR, .message = message, .inline_hints = std::move(additonal_hints) }
    );
}

void bite::Analyzer::analyze(Ast& ast) {
    this->ast = &ast;
    for (auto& stmt : ast.statements) {
        visit_stmt(stmt);
    }
}

void bite::Analyzer::block(AstNode<BlockExpr>& expr) {
    // investigate performance
    with_scope(
        [&expr, this] {
            for (auto& stmt : expr->stmts) {
                visit_stmt(stmt);
            }
            if (expr->expr) {
                visit_expr(*expr->expr);
            }
        }
    );
}

void bite::Analyzer::variable_declarataion(AstNode<VarStmt>& stmt) {
    if (stmt->value) {
        visit_expr(*stmt->value);
    }
    declare(stmt->name.string, &stmt); // TODO is it safe???
}

void bite::Analyzer::variable_expression(AstNode<VariableExpr>& expr) {
    expr->binding = resolve(expr->identifier.string, expr.span);
}

void bite::Analyzer::expression_statement(AstNode<ExprStmt>& stmt) {
    visit_expr(stmt->expr);
}

void bite::Analyzer::function_declaration(AstNode<FunctionStmt>& stmt) {
    declare_in_outer(stmt->name.string, &stmt);
    function(stmt);
}

void bite::Analyzer::function(AstNode<FunctionStmt>& stmt) {
    for (const auto& param : stmt->params) {
        declare(param.string, &stmt);
    }
    if (stmt->body) {
        visit_expr(*stmt->body);
    }
}

void bite::Analyzer::native_declaration(AstNode<NativeStmt>& stmt) {
    declare(stmt->name.string, &stmt);
}

void bite::Analyzer::class_declaration(AstNode<ClassStmt>& stmt) {
    declare(stmt->name.string, &stmt);
    auto* env = current_class_enviroment(); // assert non-null
    env->class_name = stmt->name.string;
    if (stmt->body.class_object) {
        env->class_object_enviroment = &(*stmt->body.class_object)->class_enviroment;
        node_stack.emplace_back(&(*stmt->body.class_object));
        object_expr(*stmt->body.class_object);
        node_stack.pop_back();
    }

    structure_body(stmt->body, stmt->super_class, stmt->superclass_binding, stmt->super_class_span, stmt->name_span, env, stmt->is_abstract);
}

void bite::Analyzer::object_declaration(AstNode<ObjectStmt>& stmt) {
    declare(stmt->name.string, &stmt);
    visit_expr(stmt->object);
}

void bite::Analyzer::unary(AstNode<UnaryExpr>& expr) {
    visit_expr(expr->expr);
}

void bite::Analyzer::binary(AstNode<BinaryExpr>& expr) {
    // TODO: refactor!
    // make sure to first visit right side!
    visit_expr(expr->right);
    visit_expr(expr->left);
    // Special handling for assigment
    if (expr->op == Token::Type::EQUAL || expr->op == Token::Type::PLUS_EQUAL || expr->op == Token::Type::MINUS_EQUAL ||
        expr->op == Token::Type::STAR_EQUAL || expr->op == Token::Type::SLASH_EQUAL || expr->op ==
        Token::Type::SLASH_SLASH_EQUAL || expr->op == Token::Type::AND_EQUAL || expr->op == Token::Type::CARET_EQUAL ||
        expr->op == Token::Type::BAR_EQUAL) {
        // TODO: handle more lvalues
        // TODO: FIX!
        if (std::holds_alternative<AstNode<VariableExpr>>(expr->left)) {
            expr->binding = std::get<AstNode<VariableExpr>>(expr->left)->binding;
        } else if (std::holds_alternative<AstNode<GetPropertyExpr>>(expr->left)) {
            expr->binding = PropertyBinding(std::get<AstNode<GetPropertyExpr>>(expr->left)->property.string);
        } else if (std::holds_alternative<AstNode<SuperExpr>>(expr->left)) {
            expr->binding = SuperBinding(std::get<AstNode<SuperExpr>>(expr->left)->method.string);
        } else {
            context->diagnostics.add(
                {
                    .level = DiagnosticLevel::ERROR,
                    .message = "expected lvalue",
                    .inline_hints = {
                        InlineHint {
                            .location = get_span(expr->left),
                            .message = "is not an lvalue",
                            .level = DiagnosticLevel::ERROR
                        }
                    }
                }
            );
            m_has_errors = true;
        }
    }
}

void bite::Analyzer::call(AstNode<CallExpr>& expr) {
    visit_expr(expr->callee);
    for (auto& argument : expr->arguments) {
        visit_expr(argument);
    }
}

void bite::Analyzer::get_property(AstNode<GetPropertyExpr>& expr) {
    visit_expr(expr->left);
}

void bite::Analyzer::if_expression(AstNode<IfExpr>& expr) {
    visit_expr(expr->condition);
    visit_expr(expr->then_expr);
    if (expr->else_expr) {
        visit_expr(*expr->else_expr);
    }
}

void bite::Analyzer::loop_expression(AstNode<LoopExpr>& expr) {
    block(expr->body);
}

void bite::Analyzer::break_expr(AstNode<BreakExpr>& expr) {
    // TODO better break expr analyzing!
    if (expr->label && !is_there_matching_label(expr->label->string)) {
        context->diagnostics.add(
            {
                .level = DiagnosticLevel::ERROR,
                .message = "unresolved label",
                .inline_hints = {
                    InlineHint {
                        .location = expr->label_span,
                        .message = "no matching label found",
                        .level = DiagnosticLevel::ERROR
                    }
                }
            }
        );
        m_has_errors = true;
    }
    if (expr->expr) {
        visit_expr(*expr->expr);
    }
}

void bite::Analyzer::continue_expr(AstNode<ContinueExpr>& expr) {
    if (!is_in_loop()) {
        context->diagnostics.add(
            {
                .level = DiagnosticLevel::ERROR,
                .message = "continue expression outside of loop",
                .inline_hints = {
                    InlineHint { .location = expr.span, .message = "here", .level = DiagnosticLevel::ERROR }
                }
            }
        );
        m_has_errors = true;
    }
    if (expr->label && !is_there_matching_label(expr->label->string)) {
        context->diagnostics.add(
            {
                .level = DiagnosticLevel::ERROR,
                .message = "unresolved label",
                .inline_hints = {
                    InlineHint {
                        .location = expr->label_span,
                        .message = "no matching label found",
                        .level = DiagnosticLevel::ERROR
                    }
                }
            }
        );
        m_has_errors = true;
    }
}

void bite::Analyzer::while_expr(AstNode<WhileExpr>& expr) {
    visit_expr(expr->condition);
    block(expr->body);
}

void bite::Analyzer::for_expr(AstNode<ForExpr>& expr) {
    with_scope(
        [this, &expr] {
            declare(expr->name.string, &expr);
            visit_expr(expr->iterable);
            block(expr->body);
        }
    );
}

void bite::Analyzer::return_expr(AstNode<ReturnExpr>& expr) {
    if (!is_in_function()) {
        context->diagnostics.add(
            {
                .level = DiagnosticLevel::ERROR,
                .message = "return expression outside of function",
                .inline_hints = {
                    InlineHint { .location = expr.span, .message = "here", .level = DiagnosticLevel::ERROR }
                }
            }
        );
        m_has_errors = true;
    }
    if (expr->value) {
        visit_expr(*expr->value);
    }
}

void bite::Analyzer::this_expr(AstNode<ThisExpr>& expr) {
    if (!is_in_class()) {
        context->diagnostics.add(
            {
                .level = DiagnosticLevel::ERROR,
                .message = "'this' outside of class member",
                .inline_hints = {
                    InlineHint { .location = expr.span, .message = "here", .level = DiagnosticLevel::ERROR }
                }
            }
        );
        m_has_errors = true;
    }
}

void bite::Analyzer::super_expr(const AstNode<SuperExpr>& expr) {
    if (!is_in_class_with_superclass()) {
        context->diagnostics.add(
            {
                .level = DiagnosticLevel::ERROR,
                .message = "'super' outside of class with superclass",
                .inline_hints = {
                    InlineHint { .location = expr.span, .message = "here", .level = DiagnosticLevel::ERROR }
                }
            }
        );
        m_has_errors = true;
    }
}

void bite::Analyzer::object_expr(AstNode<ObjectExpr>& expr) {
    auto* env = current_class_enviroment(); // assert non-null
    if (expr->body.class_object) {
        // TODO: better error message
        // TODO: multiline error!
        context->diagnostics.add(
            {
                .level = DiagnosticLevel::ERROR,
                .message = "object cannot contain another object",
                .inline_hints = {
                    InlineHint {
                        .location = expr->body.class_object->span,
                        .message = "",
                        .level = DiagnosticLevel::ERROR
                    }
                }
            }
        );
        m_has_errors = true;
    }
    // TODO: disallow constructors in objects
    structure_body(expr->body, expr->super_class, expr->superclass_binding, expr->super_class_span, expr->name_span, env, false);
}

void bite::Analyzer::trait_declaration(AstNode<TraitStmt>& stmt) {
    declare(stmt->name.string, &stmt);
    auto* env = current_trait_enviroment();
    unordered_dense::map<StringTable::Handle, MemberInfo> requirements; // TODO: should contain attr as well i guess?
    for (auto& using_stmt_node : stmt->using_stmts) {
        using_stmt(using_stmt_node, requirements, [this, env](StringTable::Handle name, const MemberInfo& info) {
            declare_in_trait_enviroment(*env, name, info);
        });
    }

    for (auto& field : stmt->fields) {
        declare_in_trait_enviroment(*env, field.variable->name.string, MemberInfo(field.attributes, field.span));
    }
    // hoist methods
    for (auto& method : stmt->methods) {
        declare_in_trait_enviroment(
            *env,
            method.function->name.string,
            MemberInfo(method.attributes, method.decl_span)
        );
    }
    for (auto& method : stmt->methods) {
        node_stack.emplace_back(&method.function);
        function(method.function);
        node_stack.pop_back();
    }
    // TODO: check
    // TODO: ranges
    // TODO: passing attributes
    for (auto& [name, info] : requirements) {
        if (!env->members.contains(name)) {
            env->requirements[name] = info;
        }
    }
}

DeclarationInfo* bite::Analyzer::set_declaration_info(Node node, DeclarationInfo info) {
    if (std::holds_alternative<StmtPtr>(node)) {
        auto statement = std::get<StmtPtr>(node);
        return std::visit(
            overloaded {
                [info](AstNode<VarStmt>* stmt) -> DeclarationInfo* { return &((*stmt)->info = info); },
                [](AstNode<ExprStmt>*) -> DeclarationInfo* { return nullptr; },
                [info](AstNode<FunctionStmt>* stmt) -> DeclarationInfo* { return &((*stmt)->info = info); },
                [info](AstNode<ClassStmt>* stmt) -> DeclarationInfo* { return &((*stmt)->info = info); },
                [info](AstNode<NativeStmt>* stmt) -> DeclarationInfo* { return &((*stmt)->info = info); },
                [info](AstNode<ObjectStmt>* stmt) -> DeclarationInfo* { return &((*stmt)->info = info); },
                [info](AstNode<TraitStmt>* stmt) -> DeclarationInfo* { return &((*stmt)->info = info); },
                [](AstNode<UsingStmt>*) -> DeclarationInfo* { return nullptr; },
                [](AstNode<InvalidStmt>*) -> DeclarationInfo* { return nullptr; }
            },
            statement
        );
    }
    auto expr = std::get<ExprPtr>(node);
    if (std::holds_alternative<AstNode<ForExpr>*>(expr)) {
        return &((*std::get<AstNode<ForExpr>*>(expr))->info = info);
    }
    return nullptr;
}

void bite::Analyzer::declare_in_function_enviroment(
    FunctionEnviroment& env,
    StringTable::Handle name,
    Node declaration
) {
    if (env.locals.scopes.empty()) {
        if (std::ranges::contains(env.parameters, name)) {
            // TODO: better error message, point to original
            context->diagnostics.add(
                {
                    .level = DiagnosticLevel::ERROR,
                    .message = "duplicate parameter name",
                    .inline_hints = {
                        InlineHint {
                            .location = get_span(declaration),
                            .message = "here",
                            .level = DiagnosticLevel::ERROR
                        }
                    }
                }
            );
            m_has_errors = true;
        }
        env.parameters.push_back(name);
    } else {
        for (auto& local : env.locals.scopes.back()) {
            if (local.name == name) {
                context->diagnostics.add(
                    {
                        .level = DiagnosticLevel::ERROR,
                        .message = "variable redeclared in the same scope",
                        .inline_hints = {
                            InlineHint {
                                .location = get_span(declaration),
                                .message = "new declaration here",
                                .level = DiagnosticLevel::ERROR
                            },
                            InlineHint {
                                .location = get_span(local.declaration->declaration),
                                .message = "originally declared here",
                                .level = DiagnosticLevel::INFO
                            }
                        }
                    }
                );
                m_has_errors = true;
            }
        }
        // assert not-null probably
        DeclarationInfo* info = set_declaration_info(
            declaration,
            LocalDeclarationInfo { .declaration = declaration, .idx = env.locals.locals_count++, .is_captured = false }
        );
        env.locals.scopes.back().push_back(
            Local { .declaration = reinterpret_cast<LocalDeclarationInfo*>(info), .name = name }
        );
    }
}

void bite::Analyzer::declare_in_class_enviroment(
    ClassEnviroment& env,
    StringTable::Handle name,
    const MemberInfo& attributes
) {
    if (env.members.contains(name)) {
        auto& member_attr = env.members[name];
        if (member_attr.attributes[ClassAttributes::SETTER] && !member_attr.attributes[ClassAttributes::GETTER] && !
            attributes.attributes[ClassAttributes::SETTER] && attributes.attributes[ClassAttributes::GETTER]) {
            member_attr.attributes += ClassAttributes::GETTER;
            return;
        }
        if (member_attr.attributes[ClassAttributes::GETTER] && !member_attr.attributes[ClassAttributes::SETTER] && !
            attributes.attributes[ClassAttributes::GETTER] && attributes.attributes[ClassAttributes::SETTER]) {
            member_attr.attributes += ClassAttributes::SETTER;
            return;
        }

        context->diagnostics.add(
            {
                .level = DiagnosticLevel::ERROR,
                .message = "class member name conflict",
                .inline_hints = {
                    InlineHint {
                        .location = attributes.decl_span,
                        .message = "new declaration here",
                        .level = DiagnosticLevel::ERROR
                    },
                    InlineHint {
                        .location = env.members[name].decl_span,
                        .message = "originally declared here",
                        .level = DiagnosticLevel::INFO
                    }
                }
            }
        );
        m_has_errors = true;
    }
    env.members[name] = attributes;
}

void bite::Analyzer::declare_in_trait_enviroment(
    TraitEnviroment& env,
    StringTable::Handle name,
    const MemberInfo& attributes
) {
    if (env.members.contains(name)) {
        auto& member_attr = env.members[name];
        if (member_attr.attributes[ClassAttributes::SETTER] && !member_attr.attributes[ClassAttributes::GETTER] && !
            attributes.attributes[ClassAttributes::SETTER] && attributes.attributes[ClassAttributes::GETTER]) {
            member_attr.attributes += ClassAttributes::GETTER;
            return;
        }
        if (member_attr.attributes[ClassAttributes::GETTER] && !member_attr.attributes[ClassAttributes::SETTER] && !
            attributes.attributes[ClassAttributes::GETTER] && attributes.attributes[ClassAttributes::SETTER]) {
            member_attr.attributes += ClassAttributes::SETTER;
            return;
        }
        context->diagnostics.add(
            {
                .level = DiagnosticLevel::ERROR,
                .message = "trait member name conflict",
                .inline_hints = {
                    InlineHint {
                        .location = attributes.decl_span,
                        .message = "new declaration here",
                        .level = DiagnosticLevel::ERROR
                    },
                    InlineHint {
                        .location = env.members[name].decl_span,
                        .message = "originally declared here",
                        .level = DiagnosticLevel::INFO
                    }
                }
            }
        );
        m_has_errors = true;
    }
    env.members[name] = attributes;
}

void bite::Analyzer::declare_in_global_enviroment(GlobalEnviroment& env, StringTable::Handle name, Node declaration) {
    if (env.locals.scopes.empty()) {
        if (env.globals.contains(name)) {
            context->diagnostics.add(
                {
                    .level = DiagnosticLevel::ERROR,
                    .message = "global variable name conflict",
                    .inline_hints = {
                        InlineHint {
                            .location = get_span(declaration),
                            .message = "new declaration here",
                            .level = DiagnosticLevel::ERROR
                        },
                        InlineHint {
                            .location = get_span(env.globals[name]->declaration),
                            .message = "originally declared here",
                            .level = DiagnosticLevel::INFO
                        }
                    }
                }
            );
            m_has_errors = true;
        }
        DeclarationInfo* info = set_declaration_info(
            declaration,
            GlobalDeclarationInfo { .declaration = declaration, .name = name }
        );
        env.globals[name] = reinterpret_cast<GlobalDeclarationInfo*>(info);
    } else {
        for (auto& local : env.locals.scopes.back()) {
            if (local.name == name) {
                context->diagnostics.add(
                    {
                        .level = DiagnosticLevel::ERROR,
                        .message = "variable redeclared in the same scope",
                        .inline_hints = {
                            InlineHint {
                                .location = get_span(declaration),
                                .message = "new declaration here",
                                .level = DiagnosticLevel::ERROR
                            },
                            InlineHint {
                                .location = get_span(local.declaration->declaration),
                                .message = "originally declared here",
                                .level = DiagnosticLevel::INFO
                            }
                        }
                    }
                );
                m_has_errors = true;
            }
        }
        DeclarationInfo* info = set_declaration_info(
            declaration,
            LocalDeclarationInfo { .declaration = declaration, .idx = env.locals.locals_count++, .is_captured = false }
        );
        env.locals.scopes.back().push_back(
            Local { .declaration = reinterpret_cast<LocalDeclarationInfo*>(info), .name = name }
        );
    }
}

void bite::Analyzer::declare(StringTable::Handle name, Node declaration) {
    for (auto node : node_stack | std::views::reverse) {
        if (std::holds_alternative<StmtPtr>(node)) {
            auto stmt = std::get<StmtPtr>(node);
            if (std::holds_alternative<AstNode<FunctionStmt>*>(stmt)) {
                auto* function = std::get<AstNode<FunctionStmt>*>(stmt);
                declare_in_function_enviroment((*function)->enviroment, name, declaration);
                return;
            }
        }
    }
    declare_in_global_enviroment(ast->enviroment, name, declaration);
}

ClassEnviroment* bite::Analyzer::current_class_enviroment() {
    for (auto node : node_stack | std::views::reverse) {
        if (std::holds_alternative<StmtPtr>(node)) {
            auto stmt = std::get<StmtPtr>(node);
            if (std::holds_alternative<AstNode<ClassStmt>*>(stmt)) {
                auto* klass = std::get<AstNode<ClassStmt>*>(stmt);
                return &(*klass)->enviroment;
            }
        }
        if (std::holds_alternative<ExprPtr>(node)) {
            auto expr = std::get<ExprPtr>(node);
            if (std::holds_alternative<AstNode<ObjectExpr>*>(expr)) {
                auto* object = std::get<AstNode<ObjectExpr>*>(expr);
                return &(*object)->class_enviroment;
            }
        }
    }
    return nullptr;
}

TraitEnviroment* bite::Analyzer::current_trait_enviroment() {
    for (auto node : node_stack | std::views::reverse) {
        if (std::holds_alternative<StmtPtr>(node)) {
            auto stmt = std::get<StmtPtr>(node);
            if (std::holds_alternative<AstNode<TraitStmt>*>(stmt)) {
                auto* klass = std::get<AstNode<TraitStmt>*>(stmt);
                return &(*klass)->enviroment;
            }
        }
    }
    return nullptr;
}

void bite::Analyzer::declare_in_outer(StringTable::Handle name, StmtPtr declaration) {
    bool skipped = false;
    for (auto node : node_stack | std::views::reverse) {
        if (std::holds_alternative<StmtPtr>(node)) {
            auto stmt = std::get<StmtPtr>(node);
            if (std::holds_alternative<AstNode<FunctionStmt>*>(stmt)) {
                if (!skipped) {
                    skipped = true;
                    continue;
                }
                auto* function = std::get<AstNode<FunctionStmt>*>(stmt);
                declare_in_function_enviroment((*function)->enviroment, name, declaration);
                return;
            }
        }
    }
    declare_in_global_enviroment(ast->enviroment, name, declaration);
}

std::optional<Binding> bite::Analyzer::get_binding_in_function_enviroment(
    const FunctionEnviroment& env,
    StringTable::Handle name
) {
    for (const auto& [idx, param] : env.parameters | std::views::enumerate) {
        if (param == name) {
            return ParameterBinding { idx };
        }
    }
    for (const auto& scope : env.locals.scopes | std::views::reverse) {
        for (const auto& local : scope) {
            if (local.name == name) {
                return LocalBinding { .info = local.declaration };
            }
        }
    }
    return {};
}

std::optional<Binding> bite::Analyzer::get_binding_in_class_enviroment(
    const ClassEnviroment& env,
    StringTable::Handle name,
    const SourceSpan& source
) {
    for (const auto& member : env.members | std::views::keys) {
        if (member == name) {
            return MemberBinding { member };
        }
    }
    if (env.class_object_enviroment) {
        for (const auto& member : env.class_object_enviroment->members | std::views::keys) {
            if (member == name) {
                // TODO: disallow defining anything with class name inside of class otherwise this will not work
                return ClassObjectBinding { .class_binding = resolve(env.class_name, source), .name = member };
            }
        }
    }
    return {};
}

std::optional<Binding> bite::Analyzer::get_binding_in_trait_enviroment(
    const TraitEnviroment& env,
    StringTable::Handle name
) {
    for (const auto& member : env.members | std::views::keys) {
        if (member == name) {
            return MemberBinding { member };
        }
    }
    return {};
}

std::optional<Binding> bite::Analyzer::get_binding_in_global_enviroment(
    const GlobalEnviroment& env,
    StringTable::Handle name
) {
    for (const auto& [global_name, declaration] : env.globals) {
        if (global_name == name) {
            return GlobalBinding { declaration };
        }
    }
    for (const auto& scope : env.locals.scopes | std::views::reverse) {
        for (const auto& local : scope) {
            if (local.name == name) {
                return LocalBinding { .info = local.declaration };
            }
        }
    }
    return {};
}

void bite::Analyzer::capture_local(LocalDeclarationInfo* info) {
    info->is_captured = true;
}

int64_t bite::Analyzer::add_upvalue(FunctionEnviroment& enviroment, const UpValue& upvalue) {
    auto found = std::ranges::find(enviroment.upvalues, upvalue);
    if (found != enviroment.upvalues.end()) {
        return std::distance(enviroment.upvalues.begin(), found);
    }
    enviroment.upvalues.push_back(upvalue);
    return enviroment.upvalues.size() - 1;
}

int64_t bite::Analyzer::handle_closure(
    const std::vector<std::reference_wrapper<FunctionEnviroment>>& enviroments_visited,
    StringTable::Handle name,
    int64_t local_index
) {
    bool is_local = true;
    int64_t value = local_index;
    for (const auto& enviroment : enviroments_visited | std::views::reverse) {
        value = add_upvalue(enviroment, UpValue { .idx = value, .local = is_local });
        if (is_local) {
            // Only top level can be pointing to an local variable
            is_local = false;
        }
    }
    return value;
}

Binding bite::Analyzer::resolve_without_upvalues(StringTable::Handle name, const SourceSpan& span) {
    for (auto node : node_stack | std::views::reverse) {
        if (std::holds_alternative<StmtPtr>(node)) {
            auto stmt = std::get<StmtPtr>(node);
            if (std::holds_alternative<AstNode<FunctionStmt>*>(stmt)) {
                auto* function = std::get<AstNode<FunctionStmt>*>(stmt);
                if (auto binding = get_binding_in_function_enviroment((*function)->enviroment, name)) {
                    return std::move(binding.value());
                }
            }
            if (std::holds_alternative<AstNode<ClassStmt>*>(stmt)) {
                auto& klass = std::get<AstNode<ClassStmt>*>(stmt);
                if (auto binding = get_binding_in_class_enviroment((*klass)->enviroment, name, span)) {
                    return std::move(binding.value());
                }
            }
        }
    }
    if (auto binding = get_binding_in_global_enviroment(ast->enviroment, name)) {
        return std::move(binding.value());
    }

    // TODO: better error
    context->diagnostics.add(
        {
            .level = DiagnosticLevel::ERROR,
            .message = std::format("unresolved variable: {}", *name),
            .inline_hints = { InlineHint { .location = span, .message = "here", .level = DiagnosticLevel::ERROR } }
        }
    );
    m_has_errors = true;
    return NoBinding();
}

Binding bite::Analyzer::resolve(StringTable::Handle name, const SourceSpan& span) {
    std::vector<std::reference_wrapper<FunctionEnviroment>> function_enviroments_visited;
    for (auto node : node_stack | std::views::reverse) {
        if (std::holds_alternative<StmtPtr>(node)) {
            auto stmt = std::get<StmtPtr>(node);
            if (std::holds_alternative<AstNode<FunctionStmt>*>(stmt)) {
                auto* function = std::get<AstNode<FunctionStmt>*>(stmt);
                if (auto binding = get_binding_in_function_enviroment((*function)->enviroment, name)) {
                    if (std::holds_alternative<LocalBinding>(*binding) && !function_enviroments_visited.empty()) {
                        capture_local(std::get<LocalBinding>(*binding).info);
                        return UpvalueBinding {
                                handle_closure(
                                    function_enviroments_visited,
                                    name,
                                    std::get<LocalBinding>(*binding).info->idx
                                )
                            };
                    }

                    return std::move(binding.value());
                }
                function_enviroments_visited.emplace_back((*function)->enviroment);
            }
            if (std::holds_alternative<AstNode<ClassStmt>*>(stmt)) {
                auto& klass = std::get<AstNode<ClassStmt>*>(stmt);
                if (auto binding = get_binding_in_class_enviroment((*klass)->enviroment, name, span)) {
                    return std::move(binding.value());
                }
            }
            if (std::holds_alternative<AstNode<TraitStmt>*>(stmt)) {
                auto& trait = std::get<AstNode<TraitStmt>*>(stmt);
                if (auto binding = get_binding_in_trait_enviroment((*trait)->enviroment, name)) {
                    return std::move(binding.value());
                }
            }
        } else if (std::holds_alternative<ExprPtr>(node)) {
            auto expr = std::get<ExprPtr>(node);
            if (std::holds_alternative<AstNode<ObjectExpr>*>(expr)) {
                auto& object = std::get<AstNode<ObjectExpr>*>(expr);
                if (auto binding = get_binding_in_class_enviroment((*object)->class_enviroment, name, span)) {
                    return std::move(binding.value());
                }
            }
        }
    }
    if (auto binding = get_binding_in_global_enviroment(ast->enviroment, name)) {
        if (std::holds_alternative<LocalBinding>(*binding) && !function_enviroments_visited.empty()) {
            capture_local(std::get<LocalBinding>(*binding).info);
            return UpvalueBinding {
                    handle_closure(function_enviroments_visited, name, std::get<LocalBinding>(*binding).info->idx)
                };
        }

        return std::move(binding.value());
    }
    // TODO: better error
    context->diagnostics.add(
        {
            .level = DiagnosticLevel::ERROR,
            .message = std::format("unresolved variable: {}", *name),
            .inline_hints = { InlineHint { .location = span, .message = "here", .level = DiagnosticLevel::ERROR } }
        }
    );
    m_has_errors = true;
    return NoBinding();
}

bool bite::Analyzer::is_in_loop() {
    for (auto node : node_stack) {
        if (std::holds_alternative<ExprPtr>(node)) {
            auto expr = std::get<ExprPtr>(node);
            if (std::holds_alternative<AstNode<LoopExpr>*>(expr) || std::holds_alternative<AstNode<WhileExpr>*>(expr) ||
                std::holds_alternative<AstNode<ForExpr>*>(expr)) {
                return true;
            }
        }
    }
    return false;
}

bool bite::Analyzer::is_in_function() {
    for (auto node : node_stack) {
        if (node_is_function(node)) {
            return true;
        }
    }
    return false;
}

bool bite::Analyzer::is_there_matching_label(StringTable::Handle label_name) {
    for (auto node : node_stack | std::views::reverse) {
        // labels do not cross functions
        if (node_is_function(node)) {
            break;
        }
        if (std::holds_alternative<ExprPtr>(node)) {
            auto expr = std::get<ExprPtr>(node);
            if (std::holds_alternative<AstNode<BlockExpr>*>(expr)) {
                auto label = (*std::get<AstNode<BlockExpr>*>(expr))->label;
                if (label && label->string == label_name) {
                    return true;
                }
            } else if (std::holds_alternative<AstNode<WhileExpr>*>(expr)) {
                auto label = (*std::get<AstNode<WhileExpr>*>(expr))->label;
                if (label && label->string == label_name) {
                    return true;
                }
            } else if (std::holds_alternative<AstNode<LoopExpr>*>(expr)) {
                auto label = (*std::get<AstNode<LoopExpr>*>(expr))->label;
                if (label && label->string == label_name) {
                    return true;
                }
            } else if (std::holds_alternative<AstNode<ForExpr>*>(expr)) {
                auto label = (*std::get<AstNode<ForExpr>*>(expr))->label;
                if (label && label->string == label_name) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool bite::Analyzer::is_in_class() {
    for (auto node : node_stack) {
        if (std::holds_alternative<StmtPtr>(node) && std::holds_alternative<AstNode<ClassStmt>*>(
            std::get<StmtPtr>(node)
        ) || (std::holds_alternative<ExprPtr>(node) && std::holds_alternative<AstNode<ObjectExpr>*>(
            std::get<ExprPtr>(node)
        ))) {
            return true;
        }
    }
    return false;
}

bool bite::Analyzer::is_in_class_with_superclass() {
    for (auto node : node_stack) {
        if ((std::holds_alternative<StmtPtr>(node) && std::holds_alternative<AstNode<ClassStmt>
            *>(std::get<StmtPtr>(node)) && (*std::get<AstNode<ClassStmt>*>(std::get<StmtPtr>(node)))->super_class) || (
            std::holds_alternative<ExprPtr>(node) && std::holds_alternative<AstNode<ObjectExpr>*>(
                std::get<ExprPtr>(node)
            ) && (*std::get<AstNode<ObjectExpr>*>(std::get<ExprPtr>(node)))->super_class)) {
            return true;
        }
    }
    return false;
}

std::optional<Node> bite::Analyzer::find_declaration(StringTable::Handle name, const SourceSpan& span) {
    auto binding = resolve_without_upvalues(name, span);
    if (std::holds_alternative<GlobalBinding>(binding)) {
        return std::get<GlobalBinding>(binding).info->declaration;
    }
    if (std::holds_alternative<LocalBinding>(binding)) {
        return std::get<LocalBinding>(binding).info->declaration;
    }
    return {};
}

void bite::Analyzer::check_member_declaration(
    SourceSpan& name_span,
    bool is_abstract,
    StringTable::Handle name,
    MemberInfo& info,
    unordered_dense::map<StringTable::Handle, MemberInfo>& overrideable_members
) {
    if (info.attributes[ClassAttributes::ABSTRACT] && !is_abstract) {
        emit_error_diagnostic(
            "abstract member inside of non-abstract class",
            info.decl_span,
            "is abstract",
            {
                InlineHint {
                    .location = name_span,
                    .message = "is not abstract",
                    .level = DiagnosticLevel::INFO
                }
            }
        );
    }


    if (overrideable_members.contains(name)) {
        auto& member = overrideable_members[name];
        // TODO: partial overrides!!!
        if (!info.attributes[ClassAttributes::OVERRIDE]) {
            // TODO: maybe point to original method
            emit_error_diagnostic(
                "memeber should override explicitly",
                info.decl_span,
                "add 'override' attribute to this field"
            );
        }
        // auto& member_attr = overrideable_members[method.function->name.string];
        // auto& method_attr = method.attributes;
        // if (!method.attributes[ClassAttributes::OVERRIDE] && ((member_attr.attributes[ClassAttributes::GETTER] &&
        //     method_attr[ClassAttributes::GETTER]) || (member_attr.attributes[ClassAttributes::SETTER] && method_attr
        //     [ClassAttributes::SETTER]))) {
        //     // TODO: maybe point to original method
        //     // TODO: fixme!
        //     emit_error_diagnostic(
        //         "memeber should override explicitly",
        //         method.decl_span,
        //         "add 'override' attribute to this field"
        //     );
        //     }

        // TODO: refactor!
        if (member.attributes[ClassAttributes::GETTER] && member.attributes[ClassAttributes::SETTER] && info.
            attributes[ClassAttributes::GETTER] && !info.attributes[ClassAttributes::SETTER] && info.attributes[
                ClassAttributes::OVERRIDE]) {
            member.attributes -= ClassAttributes::GETTER;
        } else if (member.attributes[ClassAttributes::GETTER] && member.attributes[ClassAttributes::SETTER] && !
            info.attributes[ClassAttributes::GETTER] && info.attributes[ClassAttributes::SETTER] && info.
            attributes[ClassAttributes::OVERRIDE]) {
            member.attributes -= ClassAttributes::SETTER;
        } else if (info.attributes[ClassAttributes::OVERRIDE]) {
            // TODO: performance?
            overrideable_members.erase(name);
        }
    } else if (info.attributes[ClassAttributes::OVERRIDE]) {
        emit_error_diagnostic(
            "memeber does not override anything",
            info.decl_span,
            "remove 'override' attribute from this field"
        );
    }
}

void bite::Analyzer::handle_constructor(
    Constructor& constructor,
    std::vector<Field>& fields,
    bool is_abstract,
    SourceSpan& name_span,
    ClassEnviroment* env,
    unordered_dense::map<StringTable::Handle, MemberInfo>& overrideable_members,
    AstNode<ClassStmt>* superclass
) {
    // TODO: we can maybe elimante has super from ClassStmt!
    if (!superclass && constructor.has_super) {
        emit_error_diagnostic(
            "no superclass to call",
            constructor.superconstructor_call_span,
            "here",
            {
                InlineHint {
                    .location = name_span,
                    .message = "does not declare any superclass",
                    .level = DiagnosticLevel::INFO
                }
            }
        );
    }

    if (superclass && (*superclass)->body.constructor) {
        auto& superconstructor = *(*superclass)->body.constructor;
        if (!superconstructor.function->params.empty() && !constructor.has_super) {
            // TODO: better diagnostic in default constructor
            // TODO: better constructor declspan
            emit_error_diagnostic(
                "subclass must call it's superclass constructor",
                constructor.decl_span,
                "must add superconstructor call here",
                {
                    InlineHint {
                        .location = name_span,
                        .message = "declares superclass here",
                        .level = DiagnosticLevel::INFO
                    },
                    InlineHint {
                        .location = superconstructor.decl_span,
                        .message = "superclass defines constructor here",
                        .level = DiagnosticLevel::INFO
                    }
                }
            );
        }
        if (constructor.super_arguments.size() != superconstructor.function->params.size()) {
            // TODO: not safe?
            emit_error_diagnostic(
                std::format(
                    "expected {} arguments, but got {} in superconstructor call",
                    superconstructor.function->params.size(),
                    constructor.super_arguments.size()
                ),
                constructor.superconstructor_call_span,
                std::format("provides {} arguments", constructor.super_arguments.size()),
                {
                    InlineHint {
                        .location = name_span,
                        .message = "superclass declared here",
                        .level = DiagnosticLevel::INFO
                    },
                    InlineHint {
                        .location = superconstructor.decl_span,
                        .message = std::format(
                            "superclass constructor expected {} arguments",
                            superconstructor.function->params.size()
                        ),
                        .level = DiagnosticLevel::INFO
                    }
                }
            );
        }
    }
    node_stack.emplace_back(&constructor.function);
    for (const auto& param : constructor.function->params) {
        declare(param.string, &constructor.function);
    }
    for (auto& super_arg : constructor.super_arguments) {
        visit_expr(super_arg);
    }
    // visit in this env to support upvalues!
    for (auto& field : fields) {
        MemberInfo info = MemberInfo(field.attributes, field.span);
        check_member_declaration(name_span, is_abstract, field.variable->name.string, info, overrideable_members);
        declare_in_class_enviroment(
            *env,
            field.variable->name.string,
            MemberInfo(field.attributes, field.span)
        );
        if (field.variable->value) {
            visit_expr(*field.variable->value);
        }
    }

    if (constructor.function->body) {
        visit_expr(*constructor.function->body);
    }
    node_stack.pop_back();
}

void bite::Analyzer::structure_body(
    StructureBody& body,
    std::optional<Token> super_class,
    Binding& superclass_binding,
    const SourceSpan& super_class_span,
    SourceSpan& name_span,
    ClassEnviroment* env,
    bool is_abstract
) {
    // TODO: init should be an reserved keyword
    unordered_dense::map<StringTable::Handle, MemberInfo> overrideable_members;
    AstNode<ClassStmt>* superclass = nullptr;
    if (super_class) {
        // TODO: refactor? better error message?
        superclass_binding = resolve(super_class->string, super_class_span);
        if (auto declaration = find_declaration(super_class->string, super_class_span)) {
            if (auto class_stmt = stmt_from_node<AstNode<ClassStmt>*>(*declaration)) {
                superclass = *class_stmt;
            } else {
                emit_error_diagnostic(
                    "superclass must be a class",
                    super_class_span,
                    "does not point to a class",
                    {
                        InlineHint {
                            .location = get_span(*declaration),
                            .message = "defined here",
                            .level = DiagnosticLevel::INFO
                        }
                    }
                );
            }
        } else {
            emit_error_diagnostic(
                "superclass must be an local or global variable",
                super_class_span,
                "is not an local or global variable"
            );
        }
        if (superclass) {
            for (auto& [name, info] : (*superclass)->enviroment.members) {
                if (info.attributes[ClassAttributes::PRIVATE]) {
                    continue;
                }
                overrideable_members[name] = info;
            }
        }
    }

    // TODO: confilcts with superclass methods and overrides
    // TODO: disallow override in trait, and validate member attributes better through whole analyzer
    unordered_dense::map<StringTable::Handle, MemberInfo> requirements;
    for (auto& using_stmt_node : body.using_statements) {
        // refactor?
        using_stmt(using_stmt_node, requirements, [this, env](StringTable::Handle name, const MemberInfo& info) {
            declare_in_class_enviroment(*env, name, info);
        });
    }

    handle_constructor(*body.constructor, body.fields, is_abstract, name_span, env, overrideable_members, superclass);

    // hoist methods
    for (const auto& method : body.methods) {
        MemberInfo info(method.attributes, method.decl_span);
        check_member_declaration(name_span, is_abstract, method.function->name.string, info, overrideable_members);
        declare_in_class_enviroment(*env, method.function->name.string, info);
    }

    // Must overrdie abstracts
    if (!is_abstract && superclass && (*superclass)->is_abstract) {
        for (const auto& [name, attr] : overrideable_members) {
            if (attr.attributes[ClassAttributes::ABSTRACT]) {
                emit_error_diagnostic(
                    std::format("abstract member {} not overriden", *name),
                    name_span,
                    "override member in this class",
                    {
                        InlineHint {
                            .location = attr.decl_span,
                            .message = "abstract member declared here",
                            .level = DiagnosticLevel::INFO
                        }
                    }
                );
            }
        }
    }

    for (const auto& [name, attr] : overrideable_members) {
        declare_in_class_enviroment(*env, name, attr);
    }

    for (auto& method : body.methods) {
        // TODO: refactor!
        node_stack.emplace_back(&method.function);
        function(method.function);
        node_stack.pop_back();
    }

    // TODO: getter and setters requirements workings
    for (auto& [requirement, info] : requirements) {
        if (!env->members.contains(requirement)) {
            // TODO: point to trait as well
            emit_error_diagnostic(
                std::format("trait requirement not satisifed: {}", *requirement),
                name_span,
                std::format("add member {} in this class", *requirement),
                {
                    InlineHint {
                        .location = info.decl_span,
                        .message = "requirement declared here",
                        .level = DiagnosticLevel::INFO
                    }
                }
            );
        }
    }
}

void bite::Analyzer::visit_stmt(Stmt& statement) {
    // TODO: refactor?
    std::visit([this](auto& stmt) { node_stack.emplace_back(&stmt); }, statement);
    std::visit(
        overloaded {
            [this](AstNode<VarStmt>& stmt) { variable_declarataion(stmt); },
            [this](AstNode<FunctionStmt>& stmt) { function_declaration(stmt); },
            [this](AstNode<ExprStmt>& stmt) { expression_statement(stmt); },
            [this](AstNode<ClassStmt>& stmt) { class_declaration(stmt); },
            [this](AstNode<NativeStmt>& stmt) { native_declaration(stmt); },
            [this](AstNode<ObjectStmt>& stmt) { object_declaration(stmt); },
            [this](AstNode<TraitStmt>& stmt) { trait_declaration(stmt); },
            [this](AstNode<UsingStmt>&) {},
            [](AstNode<InvalidStmt>&) {},
        },
        statement
    );
    node_stack.pop_back();
}

void bite::Analyzer::visit_expr(Expr& expression) {
    std::visit([this](auto& expr) { node_stack.emplace_back(&expr); }, expression);
    std::visit(
        overloaded {
            [this](AstNode<LiteralExpr>&) {},
            [this](AstNode<UnaryExpr>& expr) { unary(expr); },
            [this](AstNode<BinaryExpr>& expr) { binary(expr); },
            [this](AstNode<StringLiteral>&) {},
            [this](AstNode<VariableExpr>& expr) { variable_expression(expr); },
            [this](AstNode<CallExpr>& expr) { call(expr); },
            [this](AstNode<GetPropertyExpr>& expr) { get_property(expr); },
            [this](AstNode<SuperExpr>& expr) { super_expr(expr); },
            [this](AstNode<BlockExpr>& expr) { block(expr); },
            [this](AstNode<IfExpr>& expr) { if_expression(expr); },
            [this](AstNode<LoopExpr>& expr) { loop_expression(expr); },
            [this](AstNode<BreakExpr>& expr) { break_expr(expr); },
            [this](AstNode<ContinueExpr>& expr) { continue_expr(expr); },
            [this](AstNode<WhileExpr>& expr) { while_expr(expr); },
            [this](AstNode<ForExpr>& expr) { for_expr(expr); },
            [this](AstNode<ReturnExpr>& expr) { return_expr(expr); },
            [this](AstNode<ThisExpr>& expr) { this_expr(expr); },
            [this](AstNode<ObjectExpr>& expr) { object_expr(expr); },
            [](AstNode<InvalidExpr>&) {}
        },
        expression
    );
    node_stack.pop_back();
}
