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
    block_scopes.emplace_back(); // global scope
    for (const auto& stmt : ast.statements) {
        visit_stmt(stmt);
    }
    block_scopes.pop_back();
}

void bite::Analyzer::block(const box<BlockExpr>& expr) {
    block_scopes.emplace_back();
    for (const auto& stmt : expr->stmts) {
        visit_stmt(stmt);
    }
    block_scopes.pop_back();
}

void bite::Analyzer::variable_declarataion(const box<VarStmt>& stmt) {
    // disallow duplicate declaration in same scope
    for (auto& declaration : block_scopes.back()) {

    }

    block_scopes.back().locals.push_back(stmt->name.string);
    if (stmt->value) {
        visit_expr(*stmt->value);
    }
}

void bite::Analyzer::variable_expression(const box<VariableExpr>& expr) {
    bind(expr, expr->identifier.string);
}

void bite::Analyzer::bind(const Expr& expr, StringTable::Handle name) {
    // optim: could do that faster but is it worth find. hint: start from nearest first.
    int local_idx = 0;
    bool is_bound = false;
    for (const auto& block : block_scopes) {
        for (const auto& declaration : block.locals) {
            if (declaration == name) {
                is_bound = true;
                bindings[&expr] = Binding {.local_index = local_idx};
            }
            ++local_idx;
        }
    }
    if (!is_bound) {
        context->logger.log(Logger::Level::error, "Unbound variable!");
    }
}

void bite::Analyzer::expression_statement(const box<ExprStmt>& stmt) {
    visit_expr(stmt->expr);
}

void bite::Analyzer::visit_stmt(const Stmt& statement) {
    std::visit(overloaded {
        [this](const box<VarStmt>& stmt) { variable_declarataion(stmt); },
        [this](const box<FunctionStmt>& stmt) { },
        [this](const box<ExprStmt>& stmt) { expression_statement(stmt); },
        [this](const box<ClassStmt>& stmt) { },
        [this](const box<NativeStmt>& stmt) { },
        [this](const box<ObjectStmt>& stmt) { },
        [this](const box<TraitStmt>& stmt) { },
        [this](const box<MethodStmt>&) { },
        [this](const box<FieldStmt>&) { },
        [this](const box<ConstructorStmt>&) { },
        [this](const box<UsingStmt>&) { },
        [](const box<InvalidStmt>&) { },
    }, statement);
}

void bite::Analyzer::visit_expr(const Expr& expression) {
    std::visit(
        overloaded {
            [this](const box<LiteralExpr>& expr) { },
            [this](const box<UnaryExpr>& expr) { },
            [this](const box<BinaryExpr>& expr) { },
            [this](const box<StringLiteral>& expr) { },
            [this](const box<VariableExpr>& expr) { variable_expression(expr); },
            [this](const box<CallExpr>& expr) { },
            [this](const box<GetPropertyExpr>& expr) { },
            [this](const box<SuperExpr>& expr) { },
            [this](const box<BlockExpr>& expr) { block(expr); },
            [this](const box<IfExpr>& expr) { },
            [this](const box<LoopExpr>& expr) { },
            [this](const box<BreakExpr>& expr) { },
            [this](const box<ContinueExpr>& expr) { },
            [this](const box<WhileExpr>& expr) { },
            [this](const box<ForExpr>& expr) { },
            [this](const box<ReturnExpr>& expr) { },
            [this](const box<ThisExpr>&) { },
            [this](const box<ObjectExpr>& expr) { },
            [](const box<InvalidExpr>&) { }
        },
        expression
    );
}

