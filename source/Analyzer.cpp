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

void bite::Analyzer::block(const box<BlockExpr>& expr) {
    // investigate performance
    with_scope(
        BlockScope(),
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

void bite::Analyzer::variable_declarataion(const box<VarStmt>& stmt) {
    define(stmt->name.string);
    if (stmt->value) {
        visit_expr(*stmt->value);
    }
}

void bite::Analyzer::variable_expression(const box<VariableExpr>& expr) {
    bind(expr, expr->identifier.string);
}

void bite::Analyzer::bind(const Expr& expr, StringTable::Handle name) {
    bindings[&expr] = resolve(name);
}

void bite::Analyzer::expression_statement(const box<ExprStmt>& stmt) {
    visit_expr(stmt->expr);
}

void bite::Analyzer::function_declaration(const box<FunctionStmt>& stmt) {
    // TODO: handle captures
    define(stmt->name.string);
    with_scope(
        FunctionScope(),
        [&stmt, this] {
            for (const auto& param : stmt->params) {
                define(param.string);
            }
            if (stmt->body) {
                visit_expr(*stmt->body);
            }
        }
    );
}

void bite::Analyzer::native_declaration(const box<NativeStmt>& box) {
    define(box->name.string);
}

void bite::Analyzer::class_declaration(const box<ClassStmt>& stmt) {
    define(stmt->name.string);
    // TODO: superclasses
    // TODO: class validation!
    with_scope(
        ClassScope(),
        [&stmt, this] {
            // TODO: using statement
            // TODO: class object
            // TODO: getters and setters
            // TODO: constuctor
            for (const auto& field : stmt->body.fields) {
                define(field.variable.name.string);
                if (field.variable.value) {
                    visit_expr(*field.variable.value);
                }
            }
            // hoist methods
            for (const auto& method : stmt->body.methods) {
                define(method.function.name.string);
            }
            for (const auto& method : stmt->body.methods) {
                visit_stmt(method.function);
            }
        }
    );
}

void bite::Analyzer::unary(const box<UnaryExpr>& expr) {
    visit_expr(expr->expr);
}

void bite::Analyzer::binary(const box<BinaryExpr>& expr) {
    visit_expr(expr->left);
    visit_expr(expr->right);
}

void bite::Analyzer::call(const box<CallExpr>& expr) {
    visit_expr(expr->callee);
    for (const auto& argument : expr->arguments) {
        visit_expr(argument);
    }
}

void bite::Analyzer::get_property(const box<GetPropertyExpr>& expr) {
    visit_expr(expr->left);
}

void bite::Analyzer::if_expression(const box<IfExpr>& expr) {
    visit_expr(expr->condition);
    visit_expr(expr->then_expr);
    if (expr->else_expr) {
        visit_expr(*expr->else_expr);
    }
}

void bite::Analyzer::loop_expression(const box<LoopExpr>& expr) {
    visit_expr(expr->body);
}

void bite::Analyzer::break_expr(const box<BreakExpr>& expr) {
    if (expr->expr) {
        visit_expr(*expr->expr);
    }
}

void bite::Analyzer::while_expr(const box<WhileExpr>& expr) {
    visit_expr(expr->condition);
    visit_expr(expr->body);
}

void bite::Analyzer::for_expr(const box<ForExpr>& expr) {
    // TODO: implement
    std::unreachable();
}

void bite::Analyzer::return_expr(const box<ReturnExpr>& expr) {
    if (expr->value) {
        visit_expr(*expr->value);
    }
}

void bite::Analyzer::visit_stmt(const Stmt& statement) {
    std::visit(
        overloaded {
            [this](const box<VarStmt>& stmt) { variable_declarataion(stmt); },
            [this](const box<FunctionStmt>& stmt) { function_declaration(stmt); },
            [this](const box<ExprStmt>& stmt) { expression_statement(stmt); },
            [this](const box<ClassStmt>& stmt) { class_declaration(stmt); },
            [this](const box<NativeStmt>& stmt) { native_declaration(stmt); },
            [this](const box<ObjectStmt>& stmt) {}, // TODO: implement
            [this](const box<TraitStmt>& stmt) {}, // TODO: implement
            [this](const box<MethodStmt>&) {}, // TODO: should not exist
            [this](const box<FieldStmt>&) {}, // TODO: should not exist
            [this](const box<ConstructorStmt>&) {}, // TODO: should not exist
            [this](const box<UsingStmt>&) {}, // TODO: implement
            [](const box<InvalidStmt>&) {},
        },
        statement
    );
}

void bite::Analyzer::visit_expr(const Expr& expression) {
    std::visit(
        overloaded {
            [this](const box<LiteralExpr>&) {},
            [this](const box<UnaryExpr>& expr) {unary(expr);},
            [this](const box<BinaryExpr>& expr) {binary(expr);},
            [this](const box<StringLiteral>&) {},
            [this](const box<VariableExpr>& expr) { variable_expression(expr); },
            [this](const box<CallExpr>& expr) { call(expr); },
            [this](const box<GetPropertyExpr>& expr) { get_property(expr); },
            [this](const box<SuperExpr>& expr) {}, // TODO: implement
            [this](const box<BlockExpr>& expr) { block(expr); },
            [this](const box<IfExpr>& expr) { if_expression(expr);},
            [this](const box<LoopExpr>& expr) { loop_expression(expr); },
            [this](const box<BreakExpr>& expr) { break_expr(expr); },
            [this](const box<ContinueExpr>&) {}, // TODO: validate label
            [this](const box<WhileExpr>& expr) {while_expr(expr);},
            [this](const box<ForExpr>& expr) {for_expr(expr);}, // TODO: implement
            [this](const box<ReturnExpr>& expr) {return_expr(expr);},
            [this](const box<ThisExpr>&) {}, // TODO validate in class scope
            [this](const box<ObjectExpr>& expr) {}, // TODO: implement
            [](const box<InvalidExpr>&) {}
        },
        expression
    );
}
