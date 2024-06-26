#ifndef EXPR_H
#define EXPR_H
#include <format>
#include <memory>
#include <variant>

#include "Token.h"

// Reference: https://lesleylai.info/en/ast-in-cpp-part-1-variant/

struct UnaryExpr;
struct BinaryExpr;

struct LiteralExpr {
    int literal = 0; // todo support other literals
};

using Expr = std::variant<LiteralExpr, UnaryExpr, BinaryExpr>;
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

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

inline std::string to_string(const Expr& expr) {
    return std::visit(overloaded {
        [](const LiteralExpr& expr) {return std::to_string(expr.literal);},
            [](const UnaryExpr& expr) {return std::format("({} {})", "+", to_string(*expr.expr));},
                [](const BinaryExpr& expr) {return std::format("({} {} {})", "+", to_string(*expr.left), to_string(*expr.right));}
    }, expr);
}

#endif //EXPR_H
