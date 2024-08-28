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
        if (auto* declaration = dynamic_cast<Declaration*>(stmt.get())) {
            declare_in_global_enviroment(ast.enviroment, declaration);
        }
        if (auto* import = dynamic_cast<ImportStmt*>(stmt.get())) {
            import_stmt(*import);
        }
    }
    m_globals_hoisted = true;
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
                        visit(*expr.expr);
                    }
                }
            );
        }
    );
}

void bite::Analyzer::variable_declaration(VariableDeclaration& stmt) {
    if (stmt.value) {
        visit(*stmt.value);
    }
    declare(&stmt);
}

void bite::Analyzer::variable_expr(VariableExpr& expr) {
    expr.binding = resolve(expr.identifier.string, expr.span);
}

void bite::Analyzer::expr_stmt(ExprStmt& stmt) {
    visit(*stmt.value);
}

void bite::Analyzer::function_declaration(FunctionDeclaration& stmt) {
    declare(&stmt);
    function(stmt);
}

void bite::Analyzer::function(FunctionDeclaration& stmt) {
    with_context(
        stmt,
        [this, &stmt] {
            for (const auto& param : stmt.params) {
                // TODO: temp
                if (std::ranges::contains(stmt.enviroment.parameters, param.string)) {
                    // TODO: better error message, point to original
                    emit_error_diagnostic("duplicate parameter name", param.span, "here");
                }
                stmt.enviroment.parameters.push_back(param.string);
            }
            if (stmt.body) {
                visit(*stmt.body);
            }
        }
    );
}


void bite::Analyzer::class_declaration(ClassDeclaration& stmt) {
    declare(&stmt);
    with_context(
        stmt,
        [this, &stmt] {
            stmt.object.enviroment.class_name = stmt.name.string;
            if (stmt.object.metaobject) {
                stmt.object.enviroment.metaobject_enviroment = &stmt.object.metaobject->object.enviroment;
                with_context(
                    *stmt.object.metaobject,
                    [this, &stmt] {
                        object_expr(*stmt.object.metaobject);
                    }
                );
            }

            class_object(stmt.object, stmt.is_abstract, stmt.name.span);
        }
    );
}


void bite::Analyzer::class_object(ClassObject& object, bool is_abstract, SourceSpan& name_span) {
    unordered_dense::map<StringTable::Handle, MemberInfo> overrideable_members;
    ClassDeclaration* superclass = nullptr;
    if (object.superclass) {
        object.superclass_binding = resolve(object.superclass->string, object.superclass->span);
        if (auto declaration = find_declaration(object.superclass_binding)) {
            if (declaration && declaration.value()->is_class_declaration()) {
                superclass = declaration.value()->as_class_declaration();
            } else {
                emit_error_diagnostic(
                    "superclass must be a class",
                    object.superclass->span,
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
                object.superclass->span,
                "is not an local or global variable"
            );
        }
        if (superclass) {
            for (auto& [name, info] : superclass->object.enviroment.members) {
                if (info.attributes[ClassAttributes::PRIVATE]) {
                    continue;
                }
                overrideable_members[name] = info;
            }
        }
    }

    // TODO: disallow override and abstract in traits
    unordered_dense::map<StringTable::Handle, MemberInfo> requirements;
    for (auto& trait_used : object.traits_used) {
        // refactor?
        trait_usage(
            [&object, this, &trait_used](const auto& name, auto& info) {
                info.inclusion_span = trait_used.span;
                declare_in_class_enviroment(object.enviroment, name, info);
            },
            requirements,
            trait_used
        );
    }

    if (!superclass && object.constructor.super_arguments_call) {
        emit_error_diagnostic(
            "no superclass to call",
            object.constructor.super_arguments_call->span,
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

    if (superclass) {
        auto& superconstructor = superclass->object.constructor;
        bool has_super_constructor = superconstructor.function && !superconstructor.function->params.empty();
        if (has_super_constructor && !object.constructor.super_arguments_call) {
            if (object.constructor.function) {
                emit_error_diagnostic(
                    "subclass must call it's superclass constructor",
                    object.constructor.function->name.span,
                    "must add superconstructor to this consturctor",
                    {
                        InlineHint {
                            .location = object.superclass->span,
                            .message = "declares superclass here",
                            .level = DiagnosticLevel::INFO
                        },
                        InlineHint {
                            .location = superconstructor.function->name.span,
                            .message = "superclass defines constructor here",
                            .level = DiagnosticLevel::INFO
                        }
                    }
                );
            } else {
                emit_error_diagnostic(
                    "subclass must call it's superclass constructor",
                    name_span,
                    "add consturctor which calls superconsturtor to this class",
                    {
                        InlineHint {
                            .location = object.superclass->span,
                            .message = "declares superclass here",
                            .level = DiagnosticLevel::INFO
                        },
                        InlineHint {
                            .location = superconstructor.function->name.span,
                            .message = "superclass defines constructor here",
                            .level = DiagnosticLevel::INFO
                        }
                    }
                );
            }
        }

        if (has_super_constructor && object.constructor.super_arguments_call && object.constructor.super_arguments_call
            ->arguments.size() != superconstructor.function->params.size()) {
            emit_error_diagnostic(
                std::format(
                    "expected {} arguments, but got {} in superconstructor call",
                    superconstructor.function->params.size(),
                    object.constructor.super_arguments_call->arguments.size()
                ),
                object.constructor.super_arguments_call->span,
                std::format("provides {} arguments", object.constructor.super_arguments_call->arguments.size()),
                {
                    InlineHint {
                        .location = object.superclass->span,
                        .message = "superclass declared here",
                        .level = DiagnosticLevel::INFO
                    },
                    InlineHint {
                        .location = superconstructor.function->name.span,
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
        *object.constructor.function,
        [&] {
            if (object.constructor.function) {
                for (const auto& param : object.constructor.function->params) {
                    // TODO: temp
                    if (std::ranges::contains(object.constructor.function->enviroment.parameters, param.string)) {
                        // TODO: better error message, point to original
                        emit_error_diagnostic("duplicate parameter name", param.span, "here");
                    }
                    object.constructor.function->enviroment.parameters.push_back(param.string);
                }
            }
            if (object.constructor.super_arguments_call) {
                for (auto& super_arg : object.constructor.super_arguments_call->arguments) {
                    visit(*super_arg);
                }
            }
            // analyze in this env to support upvalues!
            for (auto& field : object.fields) {
                MemberInfo info = MemberInfo(field.attributes, field.span, name_span);
                check_member_declaration(
                    name_span,
                    is_abstract,
                    field.variable->name.string,
                    info,
                    overrideable_members,
                    false
                );
                declare_in_class_enviroment(
                    object.enviroment,
                    field.variable->name.string,
                    MemberInfo(field.attributes, field.span, name_span)
                );
                if (field.variable->value) {
                    visit(*field.variable->value);
                }
            }

            if (object.constructor.function && object.constructor.function->body) {
                visit(*object.constructor.function->body);
            }
        }
    );

    // hoist methods
    for (const auto& method : object.methods) {
        MemberInfo info(method.attributes, method.span, name_span);
        check_member_declaration(
            name_span,
            is_abstract,
            method.function->name.string,
            info,
            overrideable_members,
            true
        );
        declare_in_class_enviroment(object.enviroment, method.function->name.string, info);
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

    for (auto& [name, attr] : overrideable_members) {
        attr.inclusion_span = object.superclass->span;
        declare_in_class_enviroment(object.enviroment, name, attr);
    }

    for (auto& method : object.methods) {
        function(*method.function);
    }

    for (auto& [requirement, info] : requirements) {
        if (!object.enviroment.members.contains(requirement)) {
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
                    },
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

void bite::Analyzer::object_declaration(ObjectDeclaration& stmt) {
    declare(&stmt);
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
            expr.binding = SuperBinding(expr.left->as_super_expr()->method.string);
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
        visit(*expr.else_expr);
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
    if (expr.label) {
        if (!is_there_matching_label(expr.label->string)) {
            emit_error_diagnostic("unresolved label", expr.label->span, "no matching label found");
        }
    } else if (!is_in_loop()) {
        emit_error_diagnostic("'break' outside of loop", expr.span, "here");
    }
    if (expr.expr) {
        visit(*expr.expr);
    }
}

void bite::Analyzer::continue_expr(ContinueExpr& expr) {
    if (!is_in_loop()) {
        emit_error_diagnostic("continue expression outside of loop", expr.span, "here");
    }
    if (expr.label && !is_there_matching_label(expr.label->string)) {
        emit_error_diagnostic("unresolved label", expr.label->span, "no matching label found");
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
            visit(*expr.name);
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
        visit(*expr.value);
    }
}

void bite::Analyzer::this_expr(ThisExpr& expr) {
    if (!is_in_class()) {
        emit_error_diagnostic("'this' outside of class member", expr.span, "here");
    }
}

void bite::Analyzer::import_stmt(ImportStmt& stmt) {
    // TODO: string node should contain interned string
    auto* module = context->get_module(context->intern(stmt.module->string));

    // TODO: refactor!
    if (!module) {
        emit_error_diagnostic(std::format("module \"{}\" not found", stmt.module->string), stmt.span, "required here");
        return;
    }
    for (auto& item : stmt.items) {
        StringTable::Handle name = item->name.string;
        if (item->original_name) {
            name = item->original_name->string;
        }

        if (auto* file_module = dynamic_cast<FileModule*>(context->get_module(name))) {
            if (!file_module->declarations.contains(name)) {
                emit_error_diagnostic(
                    std::format("module \"{}\" does not declare \"{}\"", stmt.module->string, *name),
                    item->span,
                    "required here"
                );
                continue;
            }
            item->item_declaration = file_module->declarations[name];
        }
        if (auto* foreign_module = dynamic_cast<ForeignModule*>(context->get_module(name))) {
            if (!foreign_module->functions.contains(name)) {
                emit_error_diagnostic(
                    std::format("module \"{}\" does not declare \"{}\"", stmt.module->string, *name),
                    item->span,
                    "required here"
                );
                continue;
            }
        }
        declare(item.get());
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
            // TODO: better object span
            // TODO: metaobject span
            if (expr.object.metaobject) {
                emit_error_diagnostic(
                    "object cannot contain metaobject",
                    expr.object.metaobject->span,
                    "remove this",
                    { InlineHint { .location = expr.span, .message = "is an object", .level = DiagnosticLevel::INFO } }
                );
            }
            if (expr.object.constructor.function && !expr.object.constructor.function->params.empty()) {
                emit_error_diagnostic(
                    "object constructor must not declare any parameters",
                    expr.object.constructor.function->name.span,
                    ""
                );
            }
            class_object(expr.object, false, expr.span);
        }
    );
}

void bite::Analyzer::trait_declaration(TraitDeclaration& stmt) {
    declare(&stmt);
    with_context(
        stmt,
        [this, &stmt] {
            unordered_dense::map<StringTable::Handle, MemberInfo> requirements;
            for (auto& using_stmt_node : stmt.using_stmts) {
                trait_usage(
                    [&stmt, this, &using_stmt_node](const auto& name, auto& info) {
                        info.inclusion_span = using_stmt_node.span;
                        declare_in_trait_enviroment(stmt.enviroment, name, info);
                    },
                    requirements,
                    using_stmt_node
                );
            }

            for (auto& field : stmt.fields) {
                declare_in_trait_enviroment(
                    stmt.enviroment,
                    field.variable->name.string,
                    MemberInfo(field.attributes, field.span, stmt.name.span)
                );
            }
            // hoist methods
            for (auto& method : stmt.methods) {
                declare_in_trait_enviroment(
                    stmt.enviroment,
                    method.function->name.string,
                    MemberInfo(method.attributes, method.span, stmt.name.span)
                );
            }
            for (auto& method : stmt.methods) {
                function(*method.function);
            }
            for (auto& [name, info] : requirements) {
                if (!stmt.enviroment.members.contains(name)) {
                    stmt.enviroment.requirements[name] = info;
                }
            }
        }
    );
}

void bite::Analyzer::declare_local(Locals& locals, Declaration* declaration) {
    auto current_scope = locals.scopes.back();
    if (auto conflict = std::ranges::find(current_scope, declaration->name.string, &Local::name); conflict !=
        std::ranges::end(current_scope)) {
        emit_error_diagnostic(
            "variable redeclared in the same scope",
            declaration->span,
            "new declaration here",
            {
                InlineHint {
                    .location = conflict->declaration->span,
                    .message = "originally declared here",
                    .level = DiagnosticLevel::INFO
                }
            }
        );
    }
    declaration->info = LocalDeclarationInfo {
            .declaration = declaration,
            .idx = locals.locals_count++,
            .is_captured = false
        };
    locals.scopes.back().push_back(Local { .declaration = declaration, .name = declaration->name.string });
}

void bite::Analyzer::declare_in_function_enviroment(FunctionEnviroment& env, Declaration* declaration) {
    if (env.locals.scopes.empty()) {
        if (std::ranges::contains(env.parameters, declaration->name.string)) {
            // TODO: better error message, point to original
            emit_error_diagnostic("duplicate parameter name", declaration->span, "here");
        }
        env.parameters.push_back(declaration->name.string);
    } else {
        declare_local(env.locals, declaration);
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
        if (member_attr.inclusion_span) {
            if (attributes.inclusion_span) {
                emit_error_diagnostic(
                    "class member name conflict",
                    attributes.decl_span,
                    " 1. declared here",
                    {
                        InlineHint {
                            .location = member_attr.decl_span,
                            .message = "2. declared here",
                            .level = DiagnosticLevel::INFO
                        },
                        InlineHint {
                            .location = *member_attr.inclusion_span,
                            .message = "2. included from here",
                            .level = DiagnosticLevel::INFO,
                        },
                        InlineHint {
                            .location = *attributes.inclusion_span,
                            .message = "1. included from here",
                            .level = DiagnosticLevel::INFO
                        }
                    }
                );
            } else {
                emit_error_diagnostic(
                    "class member name conflict",
                    attributes.decl_span,
                    "new declaration here",
                    {
                        InlineHint {
                            .location = member_attr.decl_span,
                            .message = "originally declared here",
                            .level = DiagnosticLevel::INFO
                        },
                        InlineHint {
                            .location = *member_attr.inclusion_span,
                            .message = "included from here",
                            .level = DiagnosticLevel::INFO,
                        },
                        InlineHint {
                            .location = attributes.parent_span,
                            .message = "conflict in this class",
                            .level = DiagnosticLevel::INFO
                        }
                    }
                );
            }
        } else {
            emit_error_diagnostic(
                "class member name conflict",
                attributes.decl_span,
                "new declaration here",
                {
                    InlineHint {
                        .location = env.members[name].decl_span,
                        .message = "originally declared here",
                        .level = DiagnosticLevel::INFO
                    },
                    InlineHint {
                        .location = attributes.parent_span,
                        .message = "conflict in this class",
                        .level = DiagnosticLevel::INFO
                    }
                }
            );
        }
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
        if (member_attr.inclusion_span) {
            if (attributes.inclusion_span) {
                emit_error_diagnostic(
                    "trait member name conflict",
                    attributes.decl_span,
                    " 1. declared here",
                    {
                        InlineHint {
                            .location = member_attr.decl_span,
                            .message = "2. declared here",
                            .level = DiagnosticLevel::INFO
                        },
                        InlineHint {
                            .location = *member_attr.inclusion_span,
                            .message = "2. included from here",
                            .level = DiagnosticLevel::INFO,
                        },
                        InlineHint {
                            .location = *attributes.inclusion_span,
                            .message = "1. included from here",
                            .level = DiagnosticLevel::INFO
                        }
                    }
                );
            } else {
                emit_error_diagnostic(
                    "trait member name conflict",
                    attributes.decl_span,
                    "new declaration here",
                    {
                        InlineHint {
                            .location = member_attr.decl_span,
                            .message = "originally declared here",
                            .level = DiagnosticLevel::INFO
                        },
                        InlineHint {
                            .location = *member_attr.inclusion_span,
                            .message = "included from here",
                            .level = DiagnosticLevel::INFO,
                        },
                        InlineHint {
                            .location = attributes.parent_span,
                            .message = "conflict in this trait",
                            .level = DiagnosticLevel::INFO
                        }
                    }
                );
            }
        } else {
            emit_error_diagnostic(
                "trait member name conflict",
                attributes.decl_span,
                "new declaration here",
                {
                    InlineHint {
                        .location = env.members[name].decl_span,
                        .message = "originally declared here",
                        .level = DiagnosticLevel::INFO
                    },
                    InlineHint {
                        .location = attributes.parent_span,
                        .message = "conflict in this trait",
                        .level = DiagnosticLevel::INFO
                    }
                }
            );
        }
    }
    env.members[name] = attributes;
}

void bite::Analyzer::declare_in_global_enviroment(GlobalEnviroment& env, Declaration* declaration) {
    if (env.locals.scopes.empty()) {
        if (m_globals_hoisted) {
            return; // already declared
        }
        if (env.globals.contains(declaration->name.string)) {
            emit_error_diagnostic(
                "global variable name conflict",
                declaration->span,
                "new declaration here",
                {
                    InlineHint {
                        .location = env.globals[declaration->name.string]->span,
                        .message = "originally declared here",
                        .level = DiagnosticLevel::INFO
                    }
                }
            );
        }
        declaration->info = GlobalDeclarationInfo { .declaration = declaration, .name = declaration->name.string };
        env.globals[declaration->name.string] = declaration;
    } else {
        declare_local(env.locals, declaration);
    }
}

void bite::Analyzer::declare(Declaration* declaration) {
    for (auto* node : context_nodes | std::views::reverse) {
        if (node->is_function_declaration()) {
            declare_in_function_enviroment(node->as_function_declaration()->enviroment, declaration);
            return;
        }
    }
    declare_in_global_enviroment(ast->enviroment, declaration);
}

inline std::optional<Binding> resolve_locals(const Locals& locals, const StringTable::Handle name) {
    for (const auto& scope : locals.scopes | std::views::reverse) {
        for (const auto& local : scope) {
            if (local.name == name) {
                return LocalBinding { .info = &std::get<LocalDeclarationInfo>(local.declaration->info) };
            }
        }
    }
    return {};
}

std::optional<Binding> bite::Analyzer::resolve_in_function_enviroment(
    const FunctionEnviroment& env,
    const StringTable::Handle name
) {
    if (auto binding = resolve_locals(env.locals, name)) {
        return binding;
    }
    for (const auto& [idx, param] : env.parameters | std::views::enumerate) {
        if (param == name) {
            return ParameterBinding { idx };
        }
    }
    return {};
}

std::optional<Binding> bite::Analyzer::resolve_in_class_enviroment(
    const ClassEnviroment& env,
    const StringTable::Handle name,
    const SourceSpan& source
) {
    for (const auto& member : env.members | std::views::keys) {
        if (member == name) {
            return MemberBinding { member };
        }
    }
    if (env.metaobject_enviroment) {
        for (const auto& member : env.metaobject_enviroment->members | std::views::keys) {
            if (member == name) {
                // TODO: disallow defining anything with class name inside of class otherwise this will not work
                return ClassObjectBinding { .class_binding = resolve(env.class_name, source), .name = member };
            }
        }
    }
    return {};
}

std::optional<Binding> bite::Analyzer::resolve_in_trait_enviroment(
    const TraitEnviroment& env,
    const StringTable::Handle name
) {
    for (const auto& member : env.members | std::views::keys) {
        if (member == name) {
            return MemberBinding { member };
        }
    }
    return {};
}

std::optional<Binding> bite::Analyzer::resolve_in_global_enviroment(
    const GlobalEnviroment& env,
    StringTable::Handle name
) {
    if (auto binding = resolve_locals(env.locals, name)) {
        return binding;
    }
    for (const auto& [global_name, declaration] : env.globals) {
        if (global_name == name) {
            return GlobalBinding { .info = &std::get<GlobalDeclarationInfo>(declaration->info) };
        }
    }
    return {};
}

inline int64_t add_upvalue(FunctionEnviroment* env, const UpValue upvalue) {
    if (auto found = std::ranges::find(env->upvalues, upvalue); found != env->upvalues.end()) {
        return std::distance(env->upvalues.begin(), found);
    }
    env->upvalues.push_back(upvalue);
    return static_cast<int64_t>(env->upvalues.size() - 1);
}

Binding bite::Analyzer::propagate_upvalues(
    const LocalBinding binding,
    const std::vector<FunctionEnviroment*>& enviroments_visited
) {
    binding.info->is_captured = true;
    bool is_local = true;
    int64_t value = binding.info->idx;
    for (const auto& enviroment : enviroments_visited | std::views::reverse) {
        value = add_upvalue(enviroment, UpValue { .idx = value, .local = is_local });
        if (is_local) {
            // Only top level can be pointing to an local variable
            is_local = false;
        }
    }
    return UpvalueBinding { .idx = value, .info = binding.info };
}

Binding bite::Analyzer::resolve(StringTable::Handle name, const SourceSpan& span) {
    std::vector<FunctionEnviroment*> enviroments_visited;
    for (auto* node : context_nodes | std::views::reverse) {
        if (node->is_function_declaration()) {
            if (auto binding = resolve_in_function_enviroment(node->as_function_declaration()->enviroment, name)) {
                if (std::holds_alternative<LocalBinding>(*binding) && !enviroments_visited.empty()) {
                    return propagate_upvalues(std::get<LocalBinding>(*binding), enviroments_visited);
                }

                return std::move(binding.value());
            }
            enviroments_visited.emplace_back(&node->as_function_declaration()->enviroment);
        }
        if (node->is_class_declaration()) {
            if (auto binding = resolve_in_class_enviroment(
                node->as_class_declaration()->object.enviroment,
                name,
                span
            )) {
                return std::move(binding.value());
            }
        } else if (node->is_trait_declaration()) {
            if (auto binding = resolve_in_trait_enviroment(node->as_trait_declaration()->enviroment, name)) {
                return std::move(binding.value());
            }
        } else if (node->is_object_expr()) {
            if (auto binding = resolve_in_class_enviroment(node->as_object_expr()->object.enviroment, name, span)) {
                return std::move(binding.value());
            }
        }
    }
    if (auto binding = resolve_in_global_enviroment(ast->enviroment, name)) {
        if (std::holds_alternative<LocalBinding>(*binding) && !enviroments_visited.empty()) {
            return propagate_upvalues(std::get<LocalBinding>(*binding), enviroments_visited);
        }
        return std::move(binding.value());
    }
    emit_error_diagnostic(std::format("unresolved variable: {}", *name), span, "here");
    return NoBinding();
}

bool bite::Analyzer::is_in_loop() {
    for (auto* node : context_nodes | std::views::reverse) {
        if (node->is_function_declaration()) {
            return false;
        }
        if (node->is_loop_expr() || node->is_while_expr() || node->is_for_expr()) {
            return true;
        }
    }
    return false;
}

bool bite::Analyzer::is_in_function() {
    return std::ranges::any_of(
        context_nodes,
        [](const AstNode* node) {
            return node->is_function_declaration();
        }
    );
}

bool bite::Analyzer::is_there_matching_label(StringTable::Handle label_name) {
    for (auto* node : context_nodes | std::views::reverse) {
        // labels do not cross functions
        if (node->is_function_declaration()) {
            break;
        }
        std::optional<Token> label;
        if (node->is_block_expr()) {
            label = node->as_block_expr()->label;
        } else if (node->is_loop_expr()) {
            label = node->as_loop_expr()->label;
        } else if (node->is_while_expr()) {
            label = node->as_while_expr()->label;
        } else if (node->is_for_expr()) {
            label = node->as_for_expr()->label;
        }
        if (label && label->string == label_name) {
            return true;
        }
    }
    return false;
}

bool bite::Analyzer::is_in_class() {
    return std::ranges::any_of(
        context_nodes,
        [](const AstNode* node) { return node->is_class_declaration() || node->is_object_expr(); }
    );
}

bool bite::Analyzer::is_in_class_with_superclass() {
    for (auto* node : context_nodes) {
        if ((node->is_class_declaration() && node->as_class_declaration()->object.superclass) || (node->is_object_expr()
            && node->as_object_expr()->object.superclass)) {
            return true;
        }
    }
    return false;
}

// TODO: refactor
std::optional<AstNode*> bite::Analyzer::find_declaration(const Binding& binding) {
    AstNode* declaration = nullptr;
    if (std::holds_alternative<GlobalBinding>(binding)) {
        declaration = std::get<GlobalBinding>(binding).info->declaration;
    }
    if (std::holds_alternative<LocalBinding>(binding)) {
        declaration = std::get<LocalBinding>(binding).info->declaration;
    }
    if (std::holds_alternative<UpvalueBinding>(binding)) {
        declaration = std::get<UpvalueBinding>(binding).info->declaration;
    }
    if (!declaration) {
        return {};
    }
    if (auto* import_item = dynamic_cast<ImportStmt::Item*>(declaration)) {
        declaration = import_item->item_declaration;
    }
    if (!declaration) {
        return {};
    }
    return declaration;
}

void bite::Analyzer::check_member_declaration(
    SourceSpan& name_span,
    bool is_abstract,
    StringTable::Handle name,
    MemberInfo& info,
    unordered_dense::map<StringTable::Handle, MemberInfo>& overrideable_members,
    bool is_method
) {
    if (info.attributes[ClassAttributes::ABSTRACT] && !is_abstract) {
        emit_error_diagnostic(
            "abstract member inside of non-abstract class",
            info.decl_span,
            "is abstract",
            { InlineHint { .location = name_span, .message = "is not abstract", .level = DiagnosticLevel::INFO } }
        );
    }

    if (info.attributes[ClassAttributes::ABSTRACT] && info.attributes[ClassAttributes::PRIVATE]) {
        emit_error_diagnostic("member cannot be both abstract and private", info.decl_span, "here");
    }
    if (info.attributes[ClassAttributes::ABSTRACT] && info.attributes[ClassAttributes::OVERRIDE]) {
        emit_error_diagnostic("abstract member cannot be an override", info.decl_span, "here");
    }
    if (info.attributes[ClassAttributes::GETTER] && info.attributes[ClassAttributes::SETTER] && is_method) {
        emit_error_diagnostic("method cannot be both an getter and a setter", info.decl_span, "here");
    }
    // TODO: check getter and setter arguments count
    // TODO: better line hints

    // 1. Decide whetever is override needed or not
    bool is_override_nedded = false;
    if (overrideable_members.contains(name)) {
        auto& member = overrideable_members[name];
        if (info.attributes[ClassAttributes::GETTER] && member.attributes[ClassAttributes::GETTER]) {
            is_override_nedded = true;
        }
        if (info.attributes[ClassAttributes::SETTER] && member.attributes[ClassAttributes::SETTER]) {
            is_override_nedded = true;
        }
        if (!info.attributes[ClassAttributes::GETTER] && !info.attributes[ClassAttributes::SETTER]) {
            is_override_nedded = true;
        }
    }
    // 2. emit overrides errors
    if (is_override_nedded && !info.attributes[ClassAttributes::OVERRIDE]) {
        emit_error_diagnostic(
            "memeber should override explicitly",
            info.decl_span,
            "add 'override' attribute to this member",
            {
                InlineHint {
                    .location = overrideable_members[name].decl_span,
                    .message = "function originally declared here",
                    .level = DiagnosticLevel::INFO
                }
            }
        );
    }
    if (!is_override_nedded && info.attributes[ClassAttributes::OVERRIDE]) {
        emit_error_diagnostic(
            "memeber does not override anything",
            info.decl_span,
            "remove 'override' attribute from this field"
        );
    }
    // 3. Decide is override allowed
    if (overrideable_members.contains(name)) {
        auto& member = overrideable_members[name];
        if (info.attributes[ClassAttributes::OVERRIDE] && info.attributes[ClassAttributes::GETTER] && !member.attributes
            [ClassAttributes::GETTER]) {
            emit_error_diagnostic(
                "cannot override function with a getter",
                info.decl_span,
                "here",
                {
                    InlineHint {
                        .location = overrideable_members[name].decl_span,
                        .message = "function originally declared here",
                        .level = DiagnosticLevel::INFO
                    }
                }
            );
        }
        if (info.attributes[ClassAttributes::OVERRIDE] && info.attributes[ClassAttributes::SETTER] && !member.attributes
            [ClassAttributes::SETTER]) {
            emit_error_diagnostic(
                "cannot override function with a setter",
                info.decl_span,
                "here",
                {
                    InlineHint {
                        .location = overrideable_members[name].decl_span,
                        .message = "function originally declared here",
                        .level = DiagnosticLevel::INFO
                    }
                }
            );
        }
    }
    if (overrideable_members.contains(name)) {
        // 4. Update override list
        auto& member = overrideable_members[name];
        if (info.attributes[ClassAttributes::GETTER] || info.attributes[ClassAttributes::SETTER]) {
            if (member.attributes[ClassAttributes::GETTER] && info.attributes[ClassAttributes::GETTER]) {
                member.attributes -= ClassAttributes::GETTER;
            }
            if (member.attributes[ClassAttributes::SETTER] && info.attributes[ClassAttributes::SETTER]) {
                member.attributes -= ClassAttributes::SETTER;
            }
            if (!member.attributes[ClassAttributes::GETTER] && !member.attributes[ClassAttributes::SETTER]) {
                overrideable_members.erase(name);
            }
        } else {
            // TODO: performance?
            overrideable_members.erase(name);
        }
    }
}
