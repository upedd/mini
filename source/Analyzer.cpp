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

void bite::Analyzer::analyze(const Ast& ast) {
    for (const auto& stmt : ast.statements) {
        visit_stmt(stmt);
    }
}

void bite::Analyzer::block(const AstNode<BlockExpr>& expr) {
    // investigate performance
    with_scope(
        [&expr, this] {
            for (const auto& stmt : expr->stmts) {
                visit_stmt(stmt);
            }
            if (expr->expr) {
                visit_expr(*expr->expr);
            }
        }
    );
}

void bite::Analyzer::variable_declarataion(const AstNode<VarStmt>& stmt) {
    declare(stmt->name.string);
    if (stmt->value) {
        visit_expr(*stmt->value);
    }
    bindings[stmt.id] = get_binding(stmt->name.string);
}

void bite::Analyzer::variable_expression(const AstNode<VariableExpr>& expr) {
    bindings[expr.id] = get_binding(expr->identifier.string);
}

void bite::Analyzer::expression_statement(const AstNode<ExprStmt>& stmt) {
    visit_expr(stmt->expr);
}

void bite::Analyzer::function_declaration(const AstNode<FunctionStmt>& stmt) {
    // TODO: handle captures
    declare(stmt->name.string);
    bindings[stmt.id] = get_binding(stmt->name.string);
    with_enviroment(
        FunctionEnviroment(),
        [&stmt, this] {
            for (const auto& param : stmt->params) {
                declare(param.string);
            }
            if (stmt->body) {
                visit_expr(*stmt->body);
            }
        }
    );
}

void bite::Analyzer::native_declaration(const AstNode<NativeStmt>& box) {
    declare(box->name.string);
    bindings[box.id] = get_binding(box->name.string);
}

void bite::Analyzer::class_declaration(const AstNode<ClassStmt>& stmt) {
    declare(stmt->name.string);
    // TODO: superclasses
    // TODO: class validation!
    with_enviroment(
        ClassEnviroment(),
        [&stmt, this] {
            // TODO: using statement
            // TODO: class object
            // TODO: getters and setters
            // TODO: constuctor
            for (const auto& field : stmt->body.fields) {
                declare(field.variable->name.string);
                if (field.variable->value) {
                    visit_expr(*field.variable->value);
                }
            }
            // hoist methods
            for (const auto& method : stmt->body.methods) {
                declare(method.function->name.string);
            }
            for (const auto& method : stmt->body.methods) {
                function_declaration(method.function);
            }
        }
    );
}

void bite::Analyzer::unary(const AstNode<UnaryExpr>& expr) {
    visit_expr(expr->expr);
}

void bite::Analyzer::binary(const AstNode<BinaryExpr>& expr) {
    // TODO: handle error
    // TODO: handle other lvalues
    // TODO: maybe isolate all together?
    visit_expr(expr->left);
    if (std::holds_alternative<AstNode<VariableExpr>>(expr->left)) {
        bindings[expr.id] = bindings[std::get<AstNode<VariableExpr>>(expr->left).id];
    } else {
        bindings[expr.id] = NoBinding();
    }
    visit_expr(expr->right);
}

void bite::Analyzer::call(const AstNode<CallExpr>& expr) {
    visit_expr(expr->callee);
    for (const auto& argument : expr->arguments) {
        visit_expr(argument);
    }
}

void bite::Analyzer::get_property(const AstNode<GetPropertyExpr>& expr) {
    visit_expr(expr->left);
}

void bite::Analyzer::if_expression(const AstNode<IfExpr>& expr) {
    visit_expr(expr->condition);
    visit_expr(expr->then_expr);
    if (expr->else_expr) {
        visit_expr(*expr->else_expr);
    }
}

void bite::Analyzer::loop_expression(const AstNode<LoopExpr>& expr) {
    visit_expr(expr->body);
}

void bite::Analyzer::break_expr(const AstNode<BreakExpr>& expr) {
    if (expr->expr) {
        visit_expr(*expr->expr);
    }
}

void bite::Analyzer::while_expr(const AstNode<WhileExpr>& expr) {
    visit_expr(expr->condition);
    visit_expr(expr->body);
}

void bite::Analyzer::for_expr(const AstNode<ForExpr>& expr) {
    // TODO: implement
    std::unreachable();
}

void bite::Analyzer::return_expr(const AstNode<ReturnExpr>& expr) {
    if (expr->value) {
        visit_expr(*expr->value);
    }
}

void bite::Analyzer::visit_stmt(const Stmt& statement) {
    std::visit(
        overloaded {
            [this](const AstNode<VarStmt>& stmt) { variable_declarataion(stmt); },
            [this](const AstNode<FunctionStmt>& stmt) { function_declaration(stmt); },
            [this](const AstNode<ExprStmt>& stmt) { expression_statement(stmt); },
            [this](const AstNode<ClassStmt>& stmt) { class_declaration(stmt); },
            [this](const AstNode<NativeStmt>& stmt) { native_declaration(stmt); },
            [this](const AstNode<ObjectStmt>& stmt) {}, // TODO: implement
            [this](const AstNode<TraitStmt>& stmt) {}, // TODO: implement
            [this](const AstNode<UsingStmt>&) {}, // TODO: implement
            [](const AstNode<InvalidStmt>&) {},
        },
        statement
    );
}

void bite::Analyzer::visit_expr(const Expr& expression) {
    std::visit(
        overloaded {
            [this](const AstNode<LiteralExpr>&) {},
            [this](const AstNode<UnaryExpr>& expr) {unary(expr);},
            [this](const AstNode<BinaryExpr>& expr) {binary(expr);},
            [this](const AstNode<StringLiteral>&) {},
            [this](const AstNode<VariableExpr>& expr) { variable_expression(expr); },
            [this](const AstNode<CallExpr>& expr) { call(expr); },
            [this](const AstNode<GetPropertyExpr>& expr) { get_property(expr); },
            [this](const AstNode<SuperExpr>& expr) {}, // TODO: implement
            [this](const AstNode<BlockExpr>& expr) { block(expr); },
            [this](const AstNode<IfExpr>& expr) { if_expression(expr);},
            [this](const AstNode<LoopExpr>& expr) { loop_expression(expr); },
            [this](const AstNode<BreakExpr>& expr) { break_expr(expr); },
            [this](const AstNode<ContinueExpr>&) {}, // TODO: validate label
            [this](const AstNode<WhileExpr>& expr) {while_expr(expr);},
            [this](const AstNode<ForExpr>& expr) {for_expr(expr);}, // TODO: implement
            [this](const AstNode<ReturnExpr>& expr) {return_expr(expr);},
            [this](const AstNode<ThisExpr>&) {}, // TODO validate in class scope
            [this](const AstNode<ObjectExpr>& expr) {}, // TODO: implement
            [](const AstNode<InvalidExpr>&) {}
        },
        expression
    );
}
