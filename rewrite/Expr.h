#ifndef EXPR_H
#define EXPR_H
#include <format>
#include <memory>
#include <variant>
#include <vector>

#include "Token.h"
#include "Value.h"

// Reference: https://lesleylai.info/en/ast-in-cpp-part-1-variant/

struct UnaryExpr;
struct BinaryExpr;
struct AssigmentExpr;
struct CallExpr;

struct LiteralExpr {
    Value literal;
};

struct StringLiteral {
    std::string string;
};

struct VariableExpr {
    Token identifier;
};

using Expr = std::variant<LiteralExpr, StringLiteral, UnaryExpr, BinaryExpr, VariableExpr, AssigmentExpr, CallExpr>;
using ExprHandle = std::unique_ptr<Expr>;


struct UnaryExpr {
    ExprHandle expr = nullptr;
    Token::Type op = Token::Type::NONE; // maybe use some specific unary op enum
};

struct BinaryExpr {
    ExprHandle left = nullptr;
    ExprHandle right = nullptr;
    Token::Type op = Token::Type::NONE; // maybe use some specific binary op enum
};

struct AssigmentExpr {
    Token identifier;
    ExprHandle expr;
};

struct CallExpr {
    ExprHandle callee;
    std::vector<ExprHandle> arguments;
};

inline ExprHandle make_expr_handle(Expr expr) {
    return std::make_unique<Expr>(std::move(expr));
}

// todo refactor
inline std::string expr_to_string(const Expr& expr) {
    return std::visit(overloaded {
        [](const LiteralExpr& expr) {return expr.literal.to_string();},
        [](const UnaryExpr& expr) {return std::format("({} {})", Token::type_to_string(expr.op), expr_to_string(*expr.expr));},
        [](const BinaryExpr& expr) {return std::format("({} {} {})", Token::type_to_string(expr.op), expr_to_string(*expr.left), expr_to_string(*expr.right));},
        [](const StringLiteral& expr) {return expr.string;},
        [](const VariableExpr& expr) {return std::string("var"); },
        [](const AssigmentExpr& expr) {return std::string("assigment ") + expr_to_string(*expr.expr); },
        [](const CallExpr& expr) {return std::string("call expression!"); },

    }, expr);
}

#endif //EXPR_H
