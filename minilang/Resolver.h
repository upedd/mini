//
// Created by MiłoszK on 22.06.2024.
//

#ifndef RESOLVER_H
#define RESOLVER_H
#include <stack>

#include "Interpreter.h"
#include "generated/Expr.h"
#include "generated/Stmt.h"


class Resolver : public Expr::Visitor, public Stmt::Visitor {
public:
    explicit Resolver(Interpreter* interpreter) : interpreter(interpreter) {};

    std::any visitBlockStmt(Stmt::Block *stmt) override;
    std::any visitVarStmt(Stmt::Var *stmt) override;
    std::any visitExpressionStmt(Stmt::Expression *stmt) override;
    std::any visitIfStmt(Stmt::If *stmt) override;
    std::any visitPrintStmt(Stmt::Print *stmt) override;
    std::any visitReturnStmt(Stmt::Return *stmt) override;
    std::any visitWhileStmt(Stmt::While *stmt) override;
    std::any visitFunctionStmt(Stmt::Function *stmt) override;
    std::any visitClassStmt(Stmt::Class *stmt) override;

    std::any visitVariableExpr(Expr::Variable *expr) override;
    std::any visitAssignExpr(Expr::Assign *expr) override;
    std::any visitBinaryExpr(Expr::Binary *expr) override;
    std::any visitCallExpr(Expr::Call *expr) override;
    std::any visitGroupingExpr(Expr::Grouping *expr) override;
    std::any visitLiteralExpr(Expr::Literal *expr) override;
    std::any visitLogicalExpr(Expr::Logical *expr) override;
    std::any visitUnaryExpr(Expr::Unary *expr) override;
    std::any visitGetExpr(Expr::Get *expr) override;
    std::any visitSetExpr(Expr::Set *expr) override;
    std::any visitThisExpr(Expr::This *expr) override;
    void resolve(const std::vector<std::unique_ptr<Stmt>> & stmts);

private:
    enum class FunctionType {
        NONE,
        FUNCTION, METHOD,
        INITIALIZER
    };

    enum class ClassType {
        NONE,
        CLASS
    };

    Interpreter* interpreter;
    std::vector<std::unordered_map<std::string, bool>> scopes;
    FunctionType current_function = FunctionType::NONE;
    ClassType current_class;

    void resolve(Stmt *stmt);
    void resolve(Expr *expr);
    void resolve_local(Expr *expr, const Token &name);
    void resolve_function(Stmt::Function *function, FunctionType type);

    void begin_scope();
    void end_scope();

    void declare(const Token & name);
    void define(const Token & name);
};



#endif //RESOLVER_H
