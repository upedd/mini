#ifndef EXPR_H
#define EXPR_H
#include <memory>
#include <variant>

#include "Token.h"

// Reference: https://lesleylai.info/en/ast-in-cpp-part-1-variant/

struct UnaryExpr;
struct LiteralExpr;
struct BinaryExpr;

using Expr = std::variant<LiteralExpr, UnaryExpr, BinaryExpr>;
using ExprHandle = std::unique_ptr<Expr>;

struct LiteralExpr {
    int literal; // todo support other literals
};

struct UnaryExpr {
    ExprHandle expr;
    Token::Type op; // maybe use some specific unary op enum
};

struct BinaryExpr {
    ExprHandle left;
    ExprHandle right;
    Token::Type op; // maybe use some specific binary op enum
};

#endif //EXPR_H
