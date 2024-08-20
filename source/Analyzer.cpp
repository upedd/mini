#include "Analyzer.h"

#include "base/overloaded.h"

void bite::Analyzer::emit_message(
    const Logger::Level level,
    const std::string& content,
    const std::string& /*unused*/
) {
    if (level == Logger::Level::error) {
        m_has_errors = true;
    }
    context->logger.log(level, content);
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
    expr->binding = resolve(expr->identifier.string);
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

    // TODO: using statement
    // TODO: class object
    // TODO: getters and setters
    // TODO: init should be an reserved keyword
    unordered_dense::map<StringTable::Handle, bitflags<ClassAttributes>> overrideable_members;
    AstNode<ClassStmt>* superclass = nullptr;
    if (stmt->super_class) {
        // TODO: refactor? better error message?
        auto binding = resolve_without_upvalues(stmt->super_class->string);
        stmt->superclass_binding = resolve(stmt->super_class->string);
        if (auto* global = std::get_if<GlobalBinding>(&binding)) {
            if (std::holds_alternative<AstNode<ClassStmt>*>(global->info->declaration)) {
                superclass = std::get<AstNode<ClassStmt>*>(global->info->declaration);
            } else {
                emit_message(Logger::Level::error, "superclass must be of class type", "does not point to class type");
            }
        } else if (auto* local = std::get_if<LocalBinding>(&binding)) {
            if (std::holds_alternative<AstNode<ClassStmt>*>(local->info->declaration)) {
                superclass = std::get<AstNode<ClassStmt>*>(local->info->declaration);
            } else {
                emit_message(Logger::Level::error, "superclass must be of class type", "does not point to class type");
            }
        } else {
            emit_message(
                Logger::Level::error,
                "superclass must be a local or global variable",
                "is not a local or global variable"
            );
        }

        if (superclass) {
            for (auto& [name, attr] : (*superclass)->enviroment.members) {
                if (attr[ClassAttributes::PRIVATE]) {
                    continue;
                }
                overrideable_members[name] = attr;
            }
        }
    }
    if (!superclass && stmt->body.constructor && stmt->body.constructor->has_super) {
        emit_message(Logger::Level::error, "no superclass constructor to call", "");
    }
    // TODO: abstract constructor??
    if (stmt->body.constructor) { // TODO: always must have constructor!
        if (superclass && (*superclass)->body.constructor) {
            if (!(*superclass)->body.constructor->function->params.empty() && !stmt->body.constructor->has_super) {
                emit_message(Logger::Level::error, "subclass constructor must call superclass constructor", "here");
            }
            if (stmt->body.constructor->super_arguments.size() != (*superclass)->body.constructor->function->params.
                size()) {
                emit_message(
                    Logger::Level::error,
                    std::format(
                        "expected {} arguments, but got {}.",
                        (*superclass)->body.constructor->function->params.size(),
                        stmt->body.constructor->super_arguments.size()
                    ),
                    "here"
                );
            }
        }
        node_stack.emplace_back(&stmt->body.constructor->function);
        for (const auto& param : stmt->body.constructor->function->params) {
            declare(param.string, &stmt->body.constructor->function);
        }
        for (auto& super_arg : stmt->body.constructor->super_arguments) {
            visit_expr(super_arg);
        }
        // visit in this env to support upvalues!
        for (auto& field : stmt->body.fields) {
            if (field.attributes[ClassAttributes::ABSTRACT] && !stmt->is_abstract) {
                emit_message(Logger::Level::error, "abstract member inside of non-abstract class", "here");
            }

            // TODO: better error messages!
            if (overrideable_members.contains(field.variable->name.string)) {
                if (!field.attributes[ClassAttributes::OVERRIDE]) {
                    emit_message(
                        Logger::Level::error,
                        "member should override explicitly",
                        "add \"overrdie\" attribute"
                    );
                }
                overrideable_members.erase(field.variable->name.string);
            } else if (field.attributes[ClassAttributes::OVERRIDE]) {
                emit_message(Logger::Level::error, "member does not override anything", "remove this");
            }
            declare_in_class_enviroment(*env, field.variable->name.string, field.attributes);
            if (field.variable->value) {
                visit_expr(*field.variable->value);
            }
        }

        if (stmt->body.constructor->function->body) {
            visit_expr(*stmt->body.constructor->function->body);
        }
        node_stack.pop_back();
    }

    // hoist methods
    for (const auto& method : stmt->body.methods) {
        // TODO: deduplicate
        if (method.attributes[ClassAttributes::ABSTRACT] && !stmt->is_abstract) {
            emit_message(Logger::Level::error, "abstract member inside of non-abstract class", "here");
        }
        if (overrideable_members.contains(method.function->name.string)) {
            auto& member_attr = overrideable_members[method.function->name.string];
            auto& method_attr = method.attributes;
            // TODO: refactor me pls
            if (!method.attributes[ClassAttributes::OVERRIDE] && ((member_attr[ClassAttributes::GETTER] && method_attr[
                ClassAttributes::GETTER]) || (member_attr[ClassAttributes::SETTER] && method_attr[
                ClassAttributes::SETTER]))) {
                emit_message(Logger::Level::error, "member should override explicitly", "add \"overrdie\" attribute");
            }
            if (member_attr[ClassAttributes::GETTER] && member_attr[ClassAttributes::SETTER] && method_attr[
                ClassAttributes::GETTER] && !method_attr[ClassAttributes::SETTER] && method_attr[
                ClassAttributes::OVERRIDE]) {
                member_attr -= ClassAttributes::GETTER;
            } else if (member_attr[ClassAttributes::GETTER] && member_attr[ClassAttributes::SETTER] && !method_attr[
                ClassAttributes::GETTER] && method_attr[ClassAttributes::SETTER] && method_attr[
                ClassAttributes::OVERRIDE]) {
                member_attr -= ClassAttributes::SETTER;
            } else if (method_attr[ClassAttributes::OVERRIDE]) {
                overrideable_members.erase(method.function->name.string);
            }
        } else if (method.attributes[ClassAttributes::OVERRIDE]) {
            emit_message(Logger::Level::error, "member does not override anything", "remove this");
        }
        declare_in_class_enviroment(*env, method.function->name.string, method.attributes);
    }

    // Must overrdie abstracts
    if (!stmt->is_abstract && superclass && (*superclass)->is_abstract) {
        for (const auto& [name, attr] : overrideable_members) {
            if (attr[ClassAttributes::ABSTRACT]) {
                emit_message(Logger::Level::error, "abstract method not overrdien: " + std::string(*name), "");
            }
        }
    }

    for (const auto& [name, attr] : overrideable_members) {
        declare_in_class_enviroment(*env, name, attr);
    }

    for (auto& method : stmt->body.methods) {
        // TODO: refactor!
        node_stack.emplace_back(&method.function);
        function(method.function);
        node_stack.pop_back();
    }
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
            emit_message(Logger::Level::error, "Expected lvalue", "here");
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
        emit_message(Logger::Level::error, "no matching label found.", "referenced here");
    }
    if (expr->expr) {
        visit_expr(*expr->expr);
    }
}

void bite::Analyzer::continue_expr(AstNode<ContinueExpr>& expr) {
    if (!is_in_loop()) {
        emit_message(Logger::Level::error, "continue expression outside of loop", "here");
    }
    if (expr->label && !is_there_matching_label(expr->label->string)) {
        emit_message(Logger::Level::error, "no matching label found.", "referenced here");
    }
}

void bite::Analyzer::while_expr(AstNode<WhileExpr>& expr) {
    visit_expr(expr->condition);
    block(expr->body);
}

void bite::Analyzer::for_expr(AstNode<ForExpr>& expr) {
    // TODO: implement
    std::unreachable();
}

void bite::Analyzer::return_expr(AstNode<ReturnExpr>& expr) {
    if (!is_in_function()) {
        emit_message(Logger::Level::error, "return expression outside of function", "here");
    }
    if (expr->value) {
        visit_expr(*expr->value);
    }
}

void bite::Analyzer::this_expr(AstNode<ThisExpr>& /*unused*/) {
    if (!is_in_class()) {
        emit_message(Logger::Level::error, "'this' outside of class member", "here");
    }
}

void bite::Analyzer::super_expr(const AstNode<SuperExpr>& /*unused*/) {
    if (!is_in_class_with_superclass()) {
        emit_message(Logger::Level::error, "'super' outside of class with superclass", "here");
    }
}

void bite::Analyzer::object_expr(AstNode<ObjectExpr>& expr) {
    auto* env = current_class_enviroment(); // assert non-null
    // TODO: refactor!
    if (expr->body.class_object) {
        emit_message(Logger::Level::error, "nested object", "here");
    }
    // TODO: using statement
    // TODO: class object
    // TODO: getters and setters
    // TODO: init should be an reserved keyword
    unordered_dense::map<StringTable::Handle, bitflags<ClassAttributes>> overrideable_members;
    AstNode<ClassStmt>* superclass = nullptr;
    if (expr->super_class) {
        // TODO: refactor? better error message?
        auto binding = resolve_without_upvalues(expr->super_class->string);
        expr->superclass_binding = resolve(expr->super_class->string);
        if (auto* global = std::get_if<GlobalBinding>(&binding)) {
            if (std::holds_alternative<AstNode<ClassStmt>*>(global->info->declaration)) {
                superclass = std::get<AstNode<ClassStmt>*>(global->info->declaration);
            } else {
                emit_message(Logger::Level::error, "superclass must be of class type", "does not point to class type");
            }
        } else if (auto* local = std::get_if<LocalBinding>(&binding)) {
            if (std::holds_alternative<AstNode<ClassStmt>*>(local->info->declaration)) {
                superclass = std::get<AstNode<ClassStmt>*>(local->info->declaration);
            } else {
                emit_message(Logger::Level::error, "superclass must be of class type", "does not point to class type");
            }
        } else {
            emit_message(
                Logger::Level::error,
                "superclass must be a local or global variable",
                "is not a local or global variable"
            );
        }

        if (superclass) {
            for (auto& [name, attr] : (*superclass)->enviroment.members) {
                if (attr[ClassAttributes::PRIVATE]) {
                    continue;
                }
                overrideable_members[name] = attr;
            }
        }
    }
    if (!superclass && expr->body.constructor->has_super) {
        emit_message(Logger::Level::error, "no superclass constructor to call", "");
    }
    if (expr->body.constructor) { // TODO: always must have constructor!
        if (superclass && (*superclass)->body.constructor) {
            if (!(*superclass)->body.constructor->function->params.empty() && !expr->body.constructor->has_super) {
                emit_message(Logger::Level::error, "subclass constructor must call superclass constructor", "here");
            }
            if (expr->body.constructor->super_arguments.size() != (*superclass)->body.constructor->function->params.
                size()) {
                emit_message(
                    Logger::Level::error,
                    std::format(
                        "expected {} arguments, but got {}.",
                        (*superclass)->body.constructor->function->params.size(),
                        expr->body.constructor->super_arguments.size()
                    ),
                    "here"
                );
            }
        }
        node_stack.emplace_back(&expr->body.constructor->function);
        for (const auto& param : expr->body.constructor->function->params) {
            declare(param.string, &expr->body.constructor->function);
        }
        for (auto& super_arg : expr->body.constructor->super_arguments) {
            visit_expr(super_arg);
        }
        // visit in this env to support upvalues!
        for (auto& field : expr->body.fields) {
            if (field.attributes[ClassAttributes::ABSTRACT]) {
                emit_message(Logger::Level::error, "abstract member inside of an object", "here");
            }
            // TODO: better error messages!
            if (overrideable_members.contains(field.variable->name.string)) {
                if (!field.attributes[ClassAttributes::OVERRIDE]) {
                    emit_message(
                        Logger::Level::error,
                        "member should override explicitly",
                        "add \"overrdie\" attribute"
                    );
                }
                overrideable_members.erase(field.variable->name.string);
            } else if (field.attributes[ClassAttributes::OVERRIDE]) {
                emit_message(Logger::Level::error, "member does not override anything", "remove this");
            }
            declare_in_class_enviroment(*env, field.variable->name.string, field.attributes);
            if (field.variable->value) {
                visit_expr(*field.variable->value);
            }
        }

        if (expr->body.constructor->function->body) {
            visit_expr(*expr->body.constructor->function->body);
        }
        node_stack.pop_back();
    }

    // hoist methods
    for (const auto& method : expr->body.methods) {
        // TODO: abstract or fun before method with body in class deadlocks
        if (method.attributes[ClassAttributes::ABSTRACT]) {
            emit_message(Logger::Level::error, "abstract member inside of an object", "here");
        }
        // TODO: deduplicate
        if (overrideable_members.contains(method.function->name.string)) {
            auto& member_attr = overrideable_members[method.function->name.string];
            auto& method_attr = method.attributes;
            // TODO: refactor me pls
            if (!method.attributes[ClassAttributes::OVERRIDE] && ((member_attr[ClassAttributes::GETTER] && method_attr[
                ClassAttributes::GETTER]) || (member_attr[ClassAttributes::SETTER] && method_attr[
                ClassAttributes::SETTER]))) {
                emit_message(Logger::Level::error, "member should override explicitly", "add \"overrdie\" attribute");
            }
            if (member_attr[ClassAttributes::GETTER] && member_attr[ClassAttributes::SETTER] && method_attr[
                ClassAttributes::GETTER] && !method_attr[ClassAttributes::SETTER] && method_attr[
                ClassAttributes::OVERRIDE]) {
                member_attr -= ClassAttributes::GETTER;
            } else if (member_attr[ClassAttributes::GETTER] && member_attr[ClassAttributes::SETTER] && !method_attr[
                ClassAttributes::GETTER] && method_attr[ClassAttributes::SETTER] && method_attr[
                ClassAttributes::OVERRIDE]) {
                member_attr -= ClassAttributes::SETTER;
            } else if (method_attr[ClassAttributes::OVERRIDE]) {
                overrideable_members.erase(method.function->name.string);
            }
        } else if (method.attributes[ClassAttributes::OVERRIDE]) {
            emit_message(Logger::Level::error, "member does not override anything", "remove this");
        }
        declare_in_class_enviroment(*env, method.function->name.string, method.attributes);
    }

    // Must overrdie abstracts
    if (superclass && (*superclass)->is_abstract) {
        for (const auto& [name, attr] : overrideable_members) {
            if (attr[ClassAttributes::ABSTRACT]) {
                emit_message(Logger::Level::error, "abstract method not overrdien: " + std::string(*name), "");
            }
        }
    }

    for (const auto& [name, attr] : overrideable_members) {
        declare_in_class_enviroment(*env, name, attr);
    }

    for (auto& method : expr->body.methods) {
        // TODO: refactor!
        node_stack.emplace_back(&method.function);
        function(method.function);
        node_stack.pop_back();
    }
}

void bite::Analyzer::trait_declaration(AstNode<TraitStmt>& stmt) {
    declare(stmt->name.string, &stmt);
    auto* env = current_trait_enviroment();
    unordered_dense::set<StringTable::Handle> requirements; // TODO: should contain attr as well i guess?
    for (auto& using_stmt : stmt->using_stmts) {
        for (auto& item : using_stmt->items) {
            // Overlap with class
            // TODO: better error messages, refactor?
            auto binding = resolve_without_upvalues(item.name.string);
            AstNode<TraitStmt>* item_trait = nullptr;
            if (auto* global = std::get_if<GlobalBinding>(&binding)) {
                if (std::holds_alternative<AstNode<TraitStmt>*>(global->info->declaration)) {
                    item_trait = std::get<AstNode<TraitStmt>*>(global->info->declaration);
                } else {
                    emit_message(
                        Logger::Level::error,
                        "using item must be of trait type",
                        "does not point to trait type"
                    );
                }
            } else if (auto* local = std::get_if<LocalBinding>(&binding)) {
                if (std::holds_alternative<AstNode<TraitStmt>*>(local->info->declaration)) {
                    item_trait = std::get<AstNode<TraitStmt>*>(local->info->declaration);
                } else {
                    emit_message(
                        Logger::Level::error,
                        "using item must be of trait type",
                        "does not point to trait type"
                    );
                }
            } else {
                emit_message(
                    Logger::Level::error,
                    "Trait must be a local or global variable",
                    "is not a local or global variable"
                );
            }

            // TODO: prob refactor?
            for (auto& [field_name, field_attr] : (*item_trait)->enviroment.members) {
                //check if excluded
                bool is_excluded = false;
                // possibly use some ranges here
                for (auto& exclusion : item.exclusions) {
                    if (exclusion.string == field_name) {
                        is_excluded = true;
                        break;
                    }
                }
                if (is_excluded || field_attr[ClassAttributes::ABSTRACT]) {
                    requirements.insert(field_name);
                    // warn about useless exclude?
                    continue;
                }
                // should aliasing methods that are requierments be allowed?
                StringTable::Handle aliased_name = field_name;
                for (auto& [before, after] : item.aliases) {
                    if (before.string == field_name) {
                        aliased_name = after.string;
                        break;
                    }
                }
                declare_in_trait_enviroment(*env, aliased_name, field_attr);
            }
        }
    }

    for (auto& field : stmt->fields) {
        declare_in_trait_enviroment(*env, field.variable->name.string, field.attributes);
    }
    // hoist methods
    for (auto& method : stmt->methods) {
        declare_in_trait_enviroment(*env, method.function->name.string, method.attributes);
    }
    for (auto& method : stmt->methods) {
        node_stack.emplace_back(&method.function);
        function(method.function);
        node_stack.pop_back();
    }
    // TODO: there was some requirements section here?
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
            // TODO: implement
            [this](AstNode<UsingStmt>&) {},
            // TODO: implement
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
            // TODO: implement
            [this](AstNode<ReturnExpr>& expr) { return_expr(expr); },
            [this](AstNode<ThisExpr>& expr) { this_expr(expr); },
            [this](AstNode<ObjectExpr>& expr) { object_expr(expr); },
            [](AstNode<InvalidExpr>&) {}
        },
        expression
    );
    node_stack.pop_back();
}
