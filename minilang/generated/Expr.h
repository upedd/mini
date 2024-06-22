#ifndef Expr_H
#define Expr_H
#include "../Token.h"
#include <any>
#include <memory>

class Expr {
public:
    class Assign;
    class Binary;
    class Call;
    class Grouping;
    class Literal;
    class Logical;
    class Unary;
    class Variable;
    class Visitor {
    public:
        virtual ~Visitor() = default;
        virtual std::any visitAssignExpr(Assign* expr) = 0;
        virtual std::any visitBinaryExpr(Binary* expr) = 0;
        virtual std::any visitCallExpr(Call* expr) = 0;
        virtual std::any visitGroupingExpr(Grouping* expr) = 0;
        virtual std::any visitLiteralExpr(Literal* expr) = 0;
        virtual std::any visitLogicalExpr(Logical* expr) = 0;
        virtual std::any visitUnaryExpr(Unary* expr) = 0;
        virtual std::any visitVariableExpr(Variable* expr) = 0;
    };
    virtual std::any accept(Visitor* visitor) = 0;
    virtual ~Expr() = default;
};
class Expr::Assign : public Expr {
public:
    Assign(Token name, std::shared_ptr<Expr> value) : name(std::move(name)), value(std::move(value)) {}
    Token name; 
    std::shared_ptr<Expr> value; 

    std::any accept(Visitor* visitor) override {
        return visitor->visitAssignExpr(this);
    }
};
class Expr::Binary : public Expr {
public:
    Binary(std::shared_ptr<Expr> left, Token op, std::shared_ptr<Expr> right) : left(std::move(left)), op(std::move(op)), right(std::move(right)) {}
    std::shared_ptr<Expr> left; 
    Token op; 
    std::shared_ptr<Expr> right; 

    std::any accept(Visitor* visitor) override {
        return visitor->visitBinaryExpr(this);
    }
};
class Expr::Call : public Expr {
public:
    Call(std::shared_ptr<Expr> callee, Token paren, std::vector<std::shared_ptr<Expr>> arguments) : callee(std::move(callee)), paren(std::move(paren)), arguments(std::move(arguments)) {}
    std::shared_ptr<Expr> callee; 
    Token paren; 
    std::vector<std::shared_ptr<Expr>> arguments; 

    std::any accept(Visitor* visitor) override {
        return visitor->visitCallExpr(this);
    }
};
class Expr::Grouping : public Expr {
public:
    Grouping(std::shared_ptr<Expr> expression) : expression(std::move(expression)) {}
    std::shared_ptr<Expr> expression; 

    std::any accept(Visitor* visitor) override {
        return visitor->visitGroupingExpr(this);
    }
};
class Expr::Literal : public Expr {
public:
    Literal(std::any value) : value(std::move(value)) {}
    std::any value; 

    std::any accept(Visitor* visitor) override {
        return visitor->visitLiteralExpr(this);
    }
};
class Expr::Logical : public Expr {
public:
    Logical(std::shared_ptr<Expr> left, Token op, std::shared_ptr<Expr> right) : left(std::move(left)), op(std::move(op)), right(std::move(right)) {}
    std::shared_ptr<Expr> left; 
    Token op; 
    std::shared_ptr<Expr> right; 

    std::any accept(Visitor* visitor) override {
        return visitor->visitLogicalExpr(this);
    }
};
class Expr::Unary : public Expr {
public:
    Unary(Token op, std::shared_ptr<Expr> right) : op(std::move(op)), right(std::move(right)) {}
    Token op; 
    std::shared_ptr<Expr> right; 

    std::any accept(Visitor* visitor) override {
        return visitor->visitUnaryExpr(this);
    }
};
class Expr::Variable : public Expr {
public:
    Variable(Token name) : name(std::move(name)) {}
    Token name; 

    std::any accept(Visitor* visitor) override {
        return visitor->visitVariableExpr(this);
    }
};
#endif