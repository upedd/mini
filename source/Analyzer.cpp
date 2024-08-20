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
    declare_in_outer(stmt->name.string, &stmt);
    // TODO: superclasses
    // TODO: class validation!
    // TODO: using statement
    // TODO: class object
    // TODO: getters and setters
    // TODO: constuctor
    // TODO: init should be an reserved keyword
    // TODO: default constructor should capture upvalues
    if (stmt->body.constructor) {
        visit_expr(stmt->body.constructor->body);
    }

    for (auto& field : stmt->body.fields) {
        declare(field.variable->name.string, &stmt);
        if (field.variable->value) {
            visit_expr(*field.variable->value);
        }
    }
    // hoist methods
    for (const auto& method : stmt->body.methods) {
        declare(method.function->name.string, &stmt);
    }
    for (auto& method : stmt->body.methods) {
        // TODO: refactor!
        node_stack.emplace_back(&method.function);
        function(method.function);
        node_stack.pop_back();
    }
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
            [this](AstNode<ObjectStmt>& stmt) {},
            // TODO: implement
            [this](AstNode<TraitStmt>& stmt) {},
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
            [this](AstNode<SuperExpr>& expr) {},
            // TODO: implement
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
            // TODO validate in class scope
            [this](AstNode<ObjectExpr>& expr) {},
            // TODO: implement
            [](AstNode<InvalidExpr>&) {}
        },
        expression
    );
    node_stack.pop_back();
}
