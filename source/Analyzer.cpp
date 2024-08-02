#include "Analyzer.h"

#include "base/overloaded.h"

void bite::Analyzer::emit_message(
    const Logger::Level level,
    const std::string& content,
    const std::string& /*unused*/
) {
    //context->logger.log(level, content, nullptr); // STUB
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
                with_scope(
                    BlockScope(),
                    [&stmt, this] {
                        visit_expr(stmt->body.value());
                    }
                );
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
            for (auto& field : stmt->body.fields) {}
        }
    );
}

void bite::Analyzer::visit_stmt(const Stmt& statement) {
    std::visit(
        overloaded {
            [this](const box<VarStmt>& stmt) { variable_declarataion(stmt); },
            [this](const box<FunctionStmt>& stmt) { function_declaration(stmt); },
            [this](const box<ExprStmt>& stmt) { expression_statement(stmt); },
            [this](const box<ClassStmt>& stmt) { class_declaration(stmt); },
            [this](const box<NativeStmt>& stmt) { native_declaration(stmt); },
            [this](const box<ObjectStmt>& stmt) {},
            [this](const box<TraitStmt>& stmt) {},
            [this](const box<MethodStmt>&) {},
            [this](const box<FieldStmt>&) {},
            [this](const box<ConstructorStmt>&) {},
            [this](const box<UsingStmt>&) {},
            [](const box<InvalidStmt>&) {},
        },
        statement
    );
}

void bite::Analyzer::visit_expr(const Expr& expression) {
    std::visit(
        overloaded {
            [this](const box<LiteralExpr>& expr) {},
            [this](const box<UnaryExpr>& expr) {},
            [this](const box<BinaryExpr>& expr) {},
            [this](const box<StringLiteral>& expr) {},
            [this](const box<VariableExpr>& expr) { variable_expression(expr); },
            [this](const box<CallExpr>& expr) {},
            [this](const box<GetPropertyExpr>& expr) {},
            [this](const box<SuperExpr>& expr) {},
            [this](const box<BlockExpr>& expr) { block(expr); },
            [this](const box<IfExpr>& expr) {},
            [this](const box<LoopExpr>& expr) {},
            [this](const box<BreakExpr>& expr) {},
            [this](const box<ContinueExpr>& expr) {},
            [this](const box<WhileExpr>& expr) {},
            [this](const box<ForExpr>& expr) {},
            [this](const box<ReturnExpr>& expr) {},
            [this](const box<ThisExpr>&) {},
            [this](const box<ObjectExpr>& expr) {},
            [](const box<InvalidExpr>&) {}
        },
        expression
    );
}
