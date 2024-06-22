#ifndef Stmt_H
#define Stmt_H
#include "../Token.h"
#include "Expr.h"
#include <any>
#include <memory>

class Stmt {
public:
    class Block;
    class Expression;
    class Function;
    class If;
    class Print;
    class Return;
    class Var;
    class While;
    class Visitor {
    public:
        virtual ~Visitor() = default;
        virtual std::any visitBlockStmt(Block* stmt) = 0;
        virtual std::any visitExpressionStmt(Expression* stmt) = 0;
        virtual std::any visitFunctionStmt(Function* stmt) = 0;
        virtual std::any visitIfStmt(If* stmt) = 0;
        virtual std::any visitPrintStmt(Print* stmt) = 0;
        virtual std::any visitReturnStmt(Return* stmt) = 0;
        virtual std::any visitVarStmt(Var* stmt) = 0;
        virtual std::any visitWhileStmt(While* stmt) = 0;
    };
    virtual std::any accept(Visitor* visitor) = 0;
    virtual ~Stmt() = default;
};
class Stmt::Block : public Stmt {
public:
    Block(std::vector<std::unique_ptr<Stmt>> statements) : statements(std::move(statements)) {}
    std::vector<std::unique_ptr<Stmt>> statements; 

    std::any accept(Visitor* visitor) override {
        return visitor->visitBlockStmt(this);
    }
};
class Stmt::Expression : public Stmt {
public:
    Expression(std::unique_ptr<Expr> expression) : expression(std::move(expression)) {}
    std::unique_ptr<Expr> expression; 

    std::any accept(Visitor* visitor) override {
        return visitor->visitExpressionStmt(this);
    }
};
class Stmt::Function : public Stmt {
public:
    Function(Token name, std::vector<Token> params, std::vector<std::unique_ptr<Stmt>> body) : name(std::move(name)), params(std::move(params)), body(std::move(body)) {}
    Token name; 
    std::vector<Token> params; 
    std::vector<std::unique_ptr<Stmt>> body; 

    std::any accept(Visitor* visitor) override {
        return visitor->visitFunctionStmt(this);
    }
};
class Stmt::If : public Stmt {
public:
    If(std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> then_branch, std::unique_ptr<Stmt> else_branch) : condition(std::move(condition)), then_branch(std::move(then_branch)), else_branch(std::move(else_branch)) {}
    std::unique_ptr<Expr> condition; 
    std::unique_ptr<Stmt> then_branch; 
    std::unique_ptr<Stmt> else_branch; 

    std::any accept(Visitor* visitor) override {
        return visitor->visitIfStmt(this);
    }
};
class Stmt::Print : public Stmt {
public:
    Print(std::unique_ptr<Expr> expression) : expression(std::move(expression)) {}
    std::unique_ptr<Expr> expression; 

    std::any accept(Visitor* visitor) override {
        return visitor->visitPrintStmt(this);
    }
};
class Stmt::Return : public Stmt {
public:
    Return(Token keyword, std::unique_ptr<Expr> value) : keyword(std::move(keyword)), value(std::move(value)) {}
    Token keyword; 
    std::unique_ptr<Expr> value; 

    std::any accept(Visitor* visitor) override {
        return visitor->visitReturnStmt(this);
    }
};
class Stmt::Var : public Stmt {
public:
    Var(Token name, std::unique_ptr<Expr> initializer) : name(std::move(name)), initializer(std::move(initializer)) {}
    Token name; 
    std::unique_ptr<Expr> initializer; 

    std::any accept(Visitor* visitor) override {
        return visitor->visitVarStmt(this);
    }
};
class Stmt::While : public Stmt {
public:
    While(std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> body) : condition(std::move(condition)), body(std::move(body)) {}
    std::unique_ptr<Expr> condition; 
    std::unique_ptr<Stmt> body; 

    std::any accept(Visitor* visitor) override {
        return visitor->visitWhileStmt(this);
    }
};
#endif