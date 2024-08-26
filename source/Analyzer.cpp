#include "Analyzer.h"

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
    for (auto& stmt : ast.stmts) {
        visit(*stmt);
    }
}

void bite::Analyzer::block_expr(BlockExpr& expr) {
    // investigate performance
    with_context(
        expr,
        [&expr, this] {
            with_scope(
                [&expr, this] {
                    for (auto& stmt : expr.stmts) {
                        visit(*stmt);
                    }
                    if (expr.expr) {
                        visit(*expr.expr.value());
                    }
                }
            );
        }
    );
}

void bite::Analyzer::variable_declaration(VariableDeclaration& stmt) {
    if (stmt.value) {
        visit(*stmt.value.value());
    }
    declare(stmt.name.string, &stmt, &stmt.info); // TODO is it safe???
}

void bite::Analyzer::variable_expr(VariableExpr& expr) {
    expr.binding = resolve(expr.identifier.string, expr.span);
}

void bite::Analyzer::expr_stmt(ExprStmt& stmt) {
    visit(*stmt.value);
}

void bite::Analyzer::function_declaration(FunctionDeclaration& stmt) {
    declare_in_outer(stmt.name.string, &stmt, &stmt.info);
    function(stmt);
}

void bite::Analyzer::function(FunctionDeclaration& stmt) {
    with_context(
        stmt,
        [this, &stmt] {
            for (const auto& param : stmt.params) {
                declare(param.string, &stmt, &stmt.info);
            }
            if (stmt.body) {
                visit(*stmt.body.value());
            }
        }
    );
}

void bite::Analyzer::native_declaration(NativeDeclaration& stmt) {
    declare(stmt.name.string, &stmt, &stmt.info);
}

void bite::Analyzer::class_declaration(ClassDeclaration& stmt) {
    declare(stmt.name.string, &stmt, &stmt.info);
    with_context(
        stmt,
        [this, &stmt] {
            auto* env = current_class_enviroment(); // assert non-null
            env->class_name = stmt.name.string;
            if (stmt.body.class_object) {
                env->class_object_enviroment = &(*stmt.body.class_object)->class_enviroment;
                with_context(
                    *stmt.body.class_object.value(),
                    [this, &stmt] {
                        object_expr(*stmt.body.class_object.value());
                    }
                );
            }

            structure_body(
                stmt.body,
                stmt.super_class,
                stmt.superclass_binding,
                stmt.super_class_span,
                stmt.name_span,
                env,
                stmt.is_abstract
            );
        }
    );
}

void bite::Analyzer::object_declaration(ObjectDeclaration& stmt) {
    declare(stmt.name.string, &stmt, &stmt.info);
    visit(*stmt.object);
}

void bite::Analyzer::unary_expr(UnaryExpr& expr) {
    visit(*expr.expr);
}

void bite::Analyzer::binary_expr(BinaryExpr& expr) {
    // TODO: refactor!
    // make sure to first analyze right side!
    visit(*expr.right);
    visit(*expr.left);
    // Special handling for assigment
    if (expr.op == Token::Type::EQUAL || expr.op == Token::Type::PLUS_EQUAL || expr.op == Token::Type::MINUS_EQUAL ||
        expr.op == Token::Type::STAR_EQUAL || expr.op == Token::Type::SLASH_EQUAL || expr.op ==
        Token::Type::SLASH_SLASH_EQUAL || expr.op == Token::Type::AND_EQUAL || expr.op == Token::Type::CARET_EQUAL ||
        expr.op == Token::Type::BAR_EQUAL) {
        if (expr.left->is_variable_expr()) {
            expr.binding = expr.left->as_variable_expr()->binding;
        } else if (expr.left->is_get_property_expr()) {
            expr.binding = PropertyBinding(expr.left->as_get_property_expr()->property.string);
        } else if (expr.left->is_super_expr()) {
            expr.binding = SuperBinding(expr.as_super_expr()->method.string);
        } else {
            emit_error_diagnostic("expected lvalue", expr.left->span, "is not an lvalue");
        }
    }
}

void bite::Analyzer::call_expr(CallExpr& expr) {
    visit(*expr.callee);
    for (auto& argument : expr.arguments) {
        visit(*argument);
    }
}

void bite::Analyzer::get_property_expr(GetPropertyExpr& expr) {
    visit(*expr.left);
}

void bite::Analyzer::if_expr(IfExpr& expr) {
    visit(*expr.condition);
    visit(*expr.then_expr);
    if (expr.else_expr) {
        visit(*expr.else_expr.value());
    }
}

void bite::Analyzer::loop_expr(LoopExpr& expr) {
    with_context(
        expr,
        [this, &expr] {
            visit(*expr.body);
        }
    );
}

void bite::Analyzer::break_expr(BreakExpr& expr) {
    // TODO better break expr analyzing!
    if (expr.label && !is_there_matching_label(expr.label->string)) {
        emit_error_diagnostic("unresolved label", expr.label_span, "no matching label found");
    }
    if (expr.expr) {
        visit(*expr.expr.value());
    }
}

void bite::Analyzer::continue_expr(ContinueExpr& expr) {
    if (!is_in_loop()) {
        emit_error_diagnostic("continue expression outside of loop", expr.span, "here");
    }
    if (expr.label && !is_there_matching_label(expr.label->string)) {
        emit_error_diagnostic("unresolved label", expr.label_span, "no matching label found");
    }
}

void bite::Analyzer::while_expr(WhileExpr& expr) {
    visit(*expr.condition);
    with_context(
        expr,
        [this, &expr] {
            visit(*expr.body);
        }
    );
}

void bite::Analyzer::for_expr(ForExpr& expr) {
    with_scope(
        [this, &expr] {
            declare(expr.name.string, &expr, &expr.info);
            visit(*expr.iterable);
            with_context(
                expr,
                [this, &expr] {
                    visit(*expr.body);
                }
            );
        }
    );
}

void bite::Analyzer::return_expr(ReturnExpr& expr) {
    if (!is_in_function()) {
        emit_error_diagnostic("return expression outside of function", expr.span, "here");
    }
    if (expr.value) {
        visit(*expr.value.value());
    }
}

void bite::Analyzer::this_expr(ThisExpr& expr) {
    if (!is_in_class()) {
        emit_error_diagnostic("'this' outside of class member", expr.span, "here");
    }
}

void bite::Analyzer::super_expr(SuperExpr& expr) {
    if (!is_in_class_with_superclass()) {
        emit_error_diagnostic("'super' outside of class with superclass", expr.span, "here");
    }
}

void bite::Analyzer::object_expr(ObjectExpr& expr) {
    with_context(
        expr,
        [this, &expr] {
            auto* env = current_class_enviroment(); // assert non-null
            if (expr.body.class_object) {
                // TODO: better error message
                // TODO: multiline error!
                emit_error_diagnostic("object cannot contain another object", expr.body.class_object.value()->span, "");
            }
            // TODO: disallow constructors in objects
            structure_body(
                expr.body,
                expr.super_class,
                expr.superclass_binding,
                expr.super_class_span,
                expr.name_span,
                env,
                false
            );
        }
    );
}

void bite::Analyzer::trait_declaration(TraitDeclaration& stmt) {
    declare(stmt.name.string, &stmt, &stmt.info);
    with_context(
        stmt,
        [this, &stmt] {
            auto* env = current_trait_enviroment();
            unordered_dense::map<StringTable::Handle, MemberInfo> requirements;
            // TODO: should contain attr as well i guess?
            for (auto& using_stmt_node : stmt.using_stmts) {
                using_stmt(
                    using_stmt_node,
                    requirements,
                    [this, env](StringTable::Handle name, const MemberInfo& info) {
                        declare_in_trait_enviroment(*env, name, info);
                    }
                );
            }

            for (auto& field : stmt.fields) {
                declare_in_trait_enviroment(
                    *env,
                    field.variable->name.string,
                    MemberInfo(field.attributes, field.span)
                );
            }
            // hoist methods
            for (auto& method : stmt.methods) {
                declare_in_trait_enviroment(
                    *env,
                    method.function->name.string,
                    MemberInfo(method.attributes, method.decl_span)
                );
            }
            for (auto& method : stmt.methods) {
                visit(*method.function);
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
    );
}

void bite::Analyzer::declare_in_function_enviroment(
    FunctionEnviroment& env,
    StringTable::Handle name,
    AstNode* declaration,
    DeclarationInfo* declaration_info
) {
    if (env.locals.scopes.empty()) {
        if (std::ranges::contains(env.parameters, name)) {
            // TODO: better error message, point to original
            emit_error_diagnostic("duplicate parameter name", declaration->span, "here");
        }
        env.parameters.push_back(name);
    } else {
        for (auto& local : env.locals.scopes.back()) {
            if (local.name == name) {
                emit_error_diagnostic(
                    "variable redeclared in the same scope",
                    declaration->span,
                    "new declaration here",
                    {
                        InlineHint {
                            .location = local.declaration->declaration->span,
                            .message = "originally declared here",
                            .level = DiagnosticLevel::INFO
                        }
                    }
                );
            }
        }
        // assert not-null probably
        *declaration_info = LocalDeclarationInfo {
                .declaration = declaration,
                .idx = env.locals.locals_count++,
                .is_captured = false
            };
        env.locals.scopes.back().push_back(
            Local { .declaration = reinterpret_cast<LocalDeclarationInfo*>(declaration_info), .name = name }
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

        emit_error_diagnostic(
            "class member name conflict",
            attributes.decl_span,
            "new declaration here",
            {
                InlineHint {
                    .location = env.members[name].decl_span,
                    .message = "originally declared here",
                    .level = DiagnosticLevel::INFO
                }
            }
        );
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
        emit_error_diagnostic(
            "trait member name conflict",
            attributes.decl_span,
            "new declaration here",
            {
                InlineHint {
                    .location = env.members[name].decl_span,
                    .message = "originally declared here",
                    .level = DiagnosticLevel::INFO
                }
            }
        );
    }
    env.members[name] = attributes;
}

void bite::Analyzer::declare_in_global_enviroment(
    GlobalEnviroment& env,
    StringTable::Handle name,
    AstNode* declaration,
    DeclarationInfo* declaration_info
) {
    if (env.locals.scopes.empty()) {
        if (env.globals.contains(name)) {
            emit_error_diagnostic(
                "global variable name conflict",
                declaration->span,
                "new declaration here",
                {
                    InlineHint {
                        .location = env.globals[name]->declaration->span,
                        .message = "originally declared here",
                        .level = DiagnosticLevel::INFO
                    }
                }
            );
        }
        *declaration_info = GlobalDeclarationInfo { .declaration = declaration, .name = name };
        env.globals[name] = reinterpret_cast<GlobalDeclarationInfo*>(declaration_info);
    } else {
        for (auto& local : env.locals.scopes.back()) {
            if (local.name == name) {
                emit_error_diagnostic(
                    "variable redeclared in the same scope",
                    declaration->span,
                    "new declaration here",
                    {
                        InlineHint {
                            .location = local.declaration->declaration->span,
                            .message = "originally declared here",
                            .level = DiagnosticLevel::INFO
                        }
                    }
                );
            }
        }
        *declaration_info = LocalDeclarationInfo {
                .declaration = declaration,
                .idx = env.locals.locals_count++,
                .is_captured = false
            };
        env.locals.scopes.back().push_back(
            Local { .declaration = reinterpret_cast<LocalDeclarationInfo*>(declaration_info), .name = name }
        );
    }
}

void bite::Analyzer::declare(StringTable::Handle name, AstNode* declaration, DeclarationInfo* declaration_info) {
    for (auto* node : context_nodes | std::views::reverse) {
        if (node->is_function_declaration()) {
            declare_in_function_enviroment(
                node->as_function_declaration()->enviroment,
                name,
                declaration,
                declaration_info
            );
        }
    }
    declare_in_global_enviroment(ast->enviroment, name, declaration, declaration_info);
}

ClassEnviroment* bite::Analyzer::current_class_enviroment() {
    for (auto* node : context_nodes | std::views::reverse) {
        if (node->is_class_declaration()) {
            return &node->as_class_declaration()->enviroment;
        }
        if (node->is_object_expr()) {
            return &node->as_object_expr()->class_enviroment;
        }
    }
    return nullptr;
}

TraitEnviroment* bite::Analyzer::current_trait_enviroment() {
    for (auto* node : context_nodes | std::views::reverse) {
        if (node->is_trait_declaration()) {
            return &node->as_trait_declaration()->enviroment;
        }
    }
    return nullptr;
}

void bite::Analyzer::declare_in_outer(
    StringTable::Handle name,
    AstNode* declaration,
    DeclarationInfo* declaration_info
) {
    bool skipped = false;
    for (auto* node : context_nodes | std::views::reverse) {
        if (node->is_function_declaration()) {
            if (!skipped) {
                skipped = true;
                continue;
            }
            declare_in_function_enviroment(
                node->as_function_declaration()->enviroment,
                name,
                declaration,
                declaration_info
            );
            return;
        }
    }
    declare_in_global_enviroment(ast->enviroment, name, declaration, declaration_info);
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
    const std::vector<std::reference_wrapper<FunctionEnviroment>>& enviroments_analyzeed,
    StringTable::Handle name,
    int64_t local_index
) {
    bool is_local = true;
    int64_t value = local_index;
    for (const auto& enviroment : enviroments_analyzeed | std::views::reverse) {
        value = add_upvalue(enviroment, UpValue { .idx = value, .local = is_local });
        if (is_local) {
            // Only top level can be pointing to an local variable
            is_local = false;
        }
    }
    return value;
}

Binding bite::Analyzer::resolve_without_upvalues(StringTable::Handle name, const SourceSpan& span) {
    for (auto* node : context_nodes | std::views::reverse) {
        if (node->is_function_declaration()) {
            if (auto binding = get_binding_in_function_enviroment(node->as_function_declaration()->enviroment, name)) {
                return std::move(binding.value());
            }
        } else if (node->is_class_declaration()) {
            if (auto binding = get_binding_in_class_enviroment(node->as_class_declaration()->enviroment, name, span)) {
                return std::move(binding.value());
            }
        }
    }
    if (auto binding = get_binding_in_global_enviroment(ast->enviroment, name)) {
        return std::move(binding.value());
    }

    // TODO: better error
    emit_error_diagnostic(std::format("unresolved variable: {}", *name), span, "here");
    return NoBinding();
}

Binding bite::Analyzer::resolve(StringTable::Handle name, const SourceSpan& span) {
    std::vector<std::reference_wrapper<FunctionEnviroment>> function_enviroments_analyzeed;
    for (auto* node : context_nodes | std::views::reverse) {
        if (node->is_function_declaration()) {
            if (auto binding = get_binding_in_function_enviroment(node->as_function_declaration()->enviroment, name)) {
                if (std::holds_alternative<LocalBinding>(*binding) && !function_enviroments_analyzeed.empty()) {
                    capture_local(std::get<LocalBinding>(*binding).info);
                    return UpvalueBinding {
                            handle_closure(
                                function_enviroments_analyzeed,
                                name,
                                std::get<LocalBinding>(*binding).info->idx
                            )
                        };
                }

                return std::move(binding.value());
            }
        }
        if (node->is_class_declaration()) {
            if (auto binding = get_binding_in_class_enviroment(node->as_class_declaration()->enviroment, name, span)) {
                return std::move(binding.value());
            }
        } else if (node->is_trait_declaration()) {
            if (auto binding = get_binding_in_trait_enviroment(node->as_trait_declaration()->enviroment, name)) {
                return std::move(binding.value());
            }
        } else if (node->is_object_expr()) {
            if (auto binding = get_binding_in_class_enviroment(node->as_object_expr()->class_enviroment, name, span)) {
                return std::move(binding.value());
            }
        }
    }
    if (auto binding = get_binding_in_global_enviroment(ast->enviroment, name)) {
        if (std::holds_alternative<LocalBinding>(*binding) && !function_enviroments_analyzeed.empty()) {
            capture_local(std::get<LocalBinding>(*binding).info);
            return UpvalueBinding {
                    handle_closure(function_enviroments_analyzeed, name, std::get<LocalBinding>(*binding).info->idx)
                };
        }

        return std::move(binding.value());
    }
    // TODO: better error
    emit_error_diagnostic(std::format("unresolved variable: {}", *name), span, "here");
    return NoBinding();
}

bool bite::Analyzer::is_in_loop() {
    for (auto* node : context_nodes) {
        if (node->is_loop_expr() || node->is_while_expr() || node->is_for_expr()) {
            return true;
        }
    }
    return false;
}

bool bite::Analyzer::is_in_function() {
    for (auto* node : context_nodes) {
        if (node->is_function_declaration()) {
            return true;
        }
    }
    return false;
}

bool bite::Analyzer::is_there_matching_label(StringTable::Handle label_name) {
    for (auto* node : context_nodes | std::views::reverse) {
        // labels do not cross functions
        if (node->is_function_declaration()) {
            break;
        }
        if (node->is_block_expr()) {
            auto label = node->as_block_expr()->label;
            if (label && label->string == label_name) {
                return true;
            }
        }
        if (node->is_loop_expr()) {
            auto label = node->as_loop_expr()->label;
            if (label && label->string == label_name) {
                return true;
            }
        }
        if (node->is_while_expr()) {
            auto label = node->as_while_expr()->label;
            if (label && label->string == label_name) {
                return true;
            }
        }
        if (node->is_for_expr()) {
            auto label = node->as_for_expr()->label;
            if (label && label->string == label_name) {
                return true;
            }
        }
    }
    return false;
}

bool bite::Analyzer::is_in_class() {
    for (auto* node : context_nodes) {
        if (node->is_class_declaration() || node->is_object_expr()) {
            return true;
        }
    }
    return false;
}

bool bite::Analyzer::is_in_class_with_superclass() {
    for (auto* node : context_nodes) {
        if ((node->is_class_declaration() && node->as_class_declaration()->super_class) || (node->is_object_expr() &&
            node->as_object_expr()->super_class)) {
            return true;
        }
    }
    return false;
}

std::optional<AstNode*> bite::Analyzer::find_declaration(StringTable::Handle name, const SourceSpan& span) {
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
            { InlineHint { .location = name_span, .message = "is not abstract", .level = DiagnosticLevel::INFO } }
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
        if (member.attributes[ClassAttributes::GETTER] && member.attributes[ClassAttributes::SETTER] && info.attributes[
            ClassAttributes::GETTER] && !info.attributes[ClassAttributes::SETTER] && info.attributes[
            ClassAttributes::OVERRIDE]) {
            member.attributes -= ClassAttributes::GETTER;
        } else if (member.attributes[ClassAttributes::GETTER] && member.attributes[ClassAttributes::SETTER] && !info.
            attributes[ClassAttributes::GETTER] && info.attributes[ClassAttributes::SETTER] && info.attributes[
                ClassAttributes::OVERRIDE]) {
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
    ClassDeclaration* superclass
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

    if (superclass && superclass->body.constructor) {
        auto& superconstructor = *superclass->body.constructor;
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
        auto super_arguments_size = std::distance(
            constructor.super_arguments.begin(),
            constructor.super_arguments.end()
        );
        if (super_arguments_size != superconstructor.function->params.size()) {
            // TODO: not safe?
            emit_error_diagnostic(
                std::format(
                    "expected {} arguments, but got {} in superconstructor call",
                    superconstructor.function->params.size(),
                    super_arguments_size
                ),
                constructor.superconstructor_call_span,
                std::format("provides {} arguments", super_arguments_size),
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
    with_context(
        *constructor.function,
        [&] {
            for (const auto& param : constructor.function->params) {
                declare(param.string, &*constructor.function, &constructor.function->info);
            }

            for (auto& super_arg : constructor.super_arguments) {
                visit(*super_arg);
            }
            // analyze in this env to support upvalues!
            for (auto& field : fields) {
                MemberInfo info = MemberInfo(field.attributes, field.span);
                check_member_declaration(
                    name_span,
                    is_abstract,
                    field.variable->name.string,
                    info,
                    overrideable_members
                );
                declare_in_class_enviroment(
                    *env,
                    field.variable->name.string,
                    MemberInfo(field.attributes, field.span)
                );
                if (field.variable->value) {
                    visit(**field.variable->value);
                }
            }



            if (constructor.function->body) {
                visit(**constructor.function->body);
            }
        }
    );
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
    ClassDeclaration* superclass = nullptr;
    if (super_class) {
        // TODO: refactor? better error message?
        superclass_binding = resolve(super_class->string, super_class_span);
        if (auto declaration = find_declaration(super_class->string, super_class_span)) {
            if (declaration && declaration.value()->is_class_declaration()) {
                superclass = declaration.value()->as_class_declaration();
            } else {
                emit_error_diagnostic(
                    "superclass must be a class",
                    super_class_span,
                    "does not point to a class",
                    {
                        InlineHint {
                            .location = declaration.value()->span,
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
            for (auto& [name, info] : superclass->enviroment.members) {
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
        using_stmt(
            using_stmt_node,
            requirements,
            [this, env](StringTable::Handle name, const MemberInfo& info) {
                declare_in_class_enviroment(*env, name, info);
            }
        );
    }

    handle_constructor(*body.constructor, body.fields, is_abstract, name_span, env, overrideable_members, superclass);

    // hoist methods
    for (const auto& method : body.methods) {
        MemberInfo info(method.attributes, method.decl_span);
        check_member_declaration(name_span, is_abstract, method.function->name.string, info, overrideable_members);
        declare_in_class_enviroment(*env, method.function->name.string, info);
    }

    // Must overrdie abstracts
    if (!is_abstract && superclass && superclass->is_abstract) {
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
        with_context(*method.function, [this, &method] {
            function(*method.function);
        });
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
