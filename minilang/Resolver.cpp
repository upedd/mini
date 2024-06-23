//
// Created by MiłoszK on 22.06.2024.
//

#include "Resolver.h"

#include "Mini.h"

void Resolver::resolve(Stmt* stmt) {
    stmt->accept(this);
}

void Resolver::resolve(Expr* expr) {
    expr->accept(this);
}

void Resolver::resolve(const std::vector<std::unique_ptr<Stmt>> &stmts) {
    for (auto& stmt : stmts) {
        resolve(stmt.get());
    }
}

void Resolver::resolve_local(Expr* expr, const Token &name) {
    for (int i = scopes.size() - 1; i >= 0; i--) {
        if (scopes[i].contains(name.lexeme)) {
            interpreter->resolve(expr, scopes.size() - 1 - i);
            return;
        }
    }
}

void Resolver::begin_scope() {
    scopes.emplace_back();
}

void Resolver::end_scope() {
    scopes.pop_back();
}

void Resolver::declare(const Token &name) {
    if (scopes.empty()) return;
    auto scope = scopes.back();

    if (scope.contains(name.lexeme)) {
        Mini::error(name, "Already a variable with this name in this scope.");
    }

    scope[name.lexeme] = false;
}

void Resolver::define(const Token &name) {
    if (scopes.empty()) return;
    scopes.back()[name.lexeme] = true;
}

std::any Resolver::visitBlockStmt(Stmt::Block *stmt) {
    begin_scope();
    resolve(stmt->statements);
    end_scope();
    return {};
}

std::any Resolver::visitVarStmt(Stmt::Var *stmt) {
    declare(stmt->name);
    if (stmt->initializer != nullptr) {
        resolve(stmt->initializer.get());
    }
    define(stmt->name);
    return {};
}

std::any Resolver::visitExpressionStmt(Stmt::Expression *stmt) {
    resolve(stmt->expression.get());
    return {};
}

std::any Resolver::visitIfStmt(Stmt::If *stmt) {
    resolve(stmt->condition.get());
    resolve(stmt->then_branch.get());
    if (stmt->else_branch != nullptr) resolve(stmt->else_branch.get());
    return {};
}

std::any Resolver::visitPrintStmt(Stmt::Print *stmt) {
    resolve(stmt->expression.get());
    return {};
}

std::any Resolver::visitReturnStmt(Stmt::Return *stmt) {
    if (current_function == FunctionType::NONE) {
        Mini::error(stmt->keyword, "Can't retrun from top-level code.");
    }

    if (stmt->value != nullptr) {
        if (current_function == FunctionType::INITIALIZER) {
            Mini::error(stmt->keyword, "Can't return a value from an initializer.");
        }

        resolve(stmt->value.get());
    }
    return nullptr;
}

std::any Resolver::visitWhileStmt(Stmt::While *stmt) {
    resolve(stmt->condition.get());
    resolve(stmt->body.get());
    return {};
}

std::any Resolver::visitVariableExpr(Expr::Variable *expr) {
    if (!scopes.empty() && scopes.back().contains(expr->name.lexeme) && scopes.back()[expr->name.lexeme] == false) {
        Mini::error(expr->name, "Can't read local variable in its own initializer.");
    }

    resolve_local(expr, expr->name);
    return {};
}

std::any Resolver::visitAssignExpr(Expr::Assign *expr) {
    resolve(expr->value.get());
    resolve_local(expr, expr->name);
    return {};
}

std::any Resolver::visitBinaryExpr(Expr::Binary *expr) {
    resolve(expr->left.get());
    resolve(expr->right.get());
    return {};
}

std::any Resolver::visitCallExpr(Expr::Call *expr) {
    resolve(expr->callee.get());
    for (auto& argument : expr->arguments) {
        resolve(argument.get());
    }
    return {};
}

std::any Resolver::visitGroupingExpr(Expr::Grouping *expr) {
    resolve(expr->expression.get());
    return {};
}

std::any Resolver::visitLiteralExpr(Expr::Literal *expr) {
    return {};
}

std::any Resolver::visitLogicalExpr(Expr::Logical *expr) {
    resolve(expr->left.get());
    resolve(expr->right.get());
    return {};
}

std::any Resolver::visitUnaryExpr(Expr::Unary *expr) {
    resolve(expr->right.get());
    return {};
}

std::any Resolver::visitGetExpr(Expr::Get *expr) {
        resolve(expr->object.get());
        return {};
}

std::any Resolver::visitSetExpr(Expr::Set *expr) {
    resolve(expr->value.get());
    resolve(expr->object.get());
    return {};
}

std::any Resolver::visitThisExpr(Expr::This *expr) {
    if (current_class == ClassType::NONE) {
        Mini::error(expr->keyword, "Can't use 'this' outside of a class.");
        return {};
    }
    resolve_local(expr, expr->keyword);
    return {};
}

void Resolver::resolve_function(Stmt::Function *function, FunctionType type) {
    auto enclosing_function = current_function;
    current_function = type;

    begin_scope();
    for (auto& param : function->params) {
        declare(param);
        define(param);
    }
    resolve(function->body);
    end_scope();
    current_function = enclosing_function;
}

std::any Resolver::visitFunctionStmt(Stmt::Function *stmt) {
    declare(stmt->name);
    define(stmt->name);

    resolve_function(stmt, FunctionType::FUNCTION);
    return {};
}

std::any Resolver::visitClassStmt(Stmt::Class *stmt) {
    auto enclosing_class = current_class;
    current_class = ClassType::CLASS;
    declare(stmt->name);
    define(stmt->name);
    begin_scope();
    scopes.back()["this"] = true;

    for (auto& method : stmt->methods) {
        FunctionType declaration = FunctionType::METHOD;
        if (method->name.lexeme == "init") {
            declaration = FunctionType::INITIALIZER;
        }
        resolve_function(method.get(), declaration);
    }

    end_scope();
    current_class = enclosing_class;
    return {};
}
