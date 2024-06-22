
#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <chrono>
#include <functional>

#include "Enviroment.h"
#include "generated/Expr.h"
#include "generated/Stmt.h"


template<typename T>
class FIXME {
public:
    explicit FIXME(std::function<T> func) : func(std::move(func)) {};
    std::function<T> func;
    ~FIXME() {
        func();
    }
};

class Interpreter : public Expr::Visitor, public Stmt::Visitor {
public:
    Interpreter();

    void interpret(const std::vector<std::unique_ptr<Stmt>>& statements);

    std::any visitLiteralExpr(Expr::Literal *expr) override;
    std::any visitGroupingExpr(Expr::Grouping *expr) override;
    std::any visitUnaryExpr(Expr::Unary *expr) override;
    std::any visitBinaryExpr(Expr::Binary *expr) override;
    std::any visitExpressionStmt(Stmt::Expression *stmt) override;
    std::any visitPrintStmt(Stmt::Print *stmt) override;
    std::any visitVarStmt(Stmt::Var *stmt) override;

    std::any look_up_variable(const Token& token, Expr* expr);

    std::any visitVariableExpr(Expr::Variable *expr) override;
    std::any visitAssignExpr(Expr::Assign *expr) override;
    std::any visitBlockStmt(Stmt::Block *stmt) override;
    std::any visitIfStmt(Stmt::If *stmt) override;
    std::any visitLogicalExpr(Expr::Logical *expr) override;
    std::any visitWhileStmt(Stmt::While *stmt) override;
    std::any visitCallExpr(Expr::Call *expr) override;
    std::any visitFunctionStmt(Stmt::Function *stmt) override;
    std::any visitReturnStmt(Stmt::Return *stmt) override;

    void execute_block(const std::vector<std::unique_ptr<Stmt>> &stmts, Enviroment env);

    void resolve(Expr *expr, int depth);

    std::shared_ptr<Enviroment> globals = std::make_shared<Enviroment>();


private:
    std::any evaluate(Expr* expr);
    void execute(Stmt* get);
    void check_number_operand(Token op, const std::any& operand);
    void check_number_operands(Token op, const std::any& left, const std::any& right);
    static bool isTruthy(const std::any& object);
    static bool isEqual(const std::any & any, const std::any & right);
    std::string stringify(const std::any & object);
    std::shared_ptr<Enviroment> enviroment;
    std::unordered_map<Expr*, int> locals;
};


#endif //INTERPRETER_H
