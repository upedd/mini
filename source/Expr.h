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
struct LiteralExpr;
struct StringLiteral;
struct VariableExpr;
struct SetPropertyExpr;
struct GetPropertyExpr;
struct SuperExpr;

using Expr = std::variant<LiteralExpr, StringLiteral, UnaryExpr, BinaryExpr, VariableExpr, AssigmentExpr, CallExpr, SetPropertyExpr, GetPropertyExpr, SuperExpr>;
using ExprHandle = std::unique_ptr<Expr>;

struct UnaryExpr {
    ExprHandle expr;
    Token::Type op;
};

struct BinaryExpr {
    ExprHandle left;
    ExprHandle right;
    Token::Type op;
};

struct AssigmentExpr {
    Token identifier;
    ExprHandle expr;
};

struct CallExpr {
    ExprHandle callee;
    std::vector<ExprHandle> arguments;
};

struct LiteralExpr {
    Value literal;
};

struct StringLiteral {
    std::string string;
};

struct VariableExpr {
    Token identifier;
};

struct GetPropertyExpr {
    ExprHandle left; // Todo: better name?
    Token property;
};

struct SetPropertyExpr {
    ExprHandle left;
    Token property;
    ExprHandle expression;
};

struct SuperExpr {
    Token method;
};

inline ExprHandle make_expr_handle(Expr expr) {
    return std::make_unique<Expr>(std::move(expr));
}

inline std::string expr_to_string(const Expr &expr, std::string_view source) {
    return std::visit(overloaded{
                          [](const LiteralExpr &expr) {
                              return expr.literal.to_string();
                          },
                          [source](const UnaryExpr &expr) {
                              return std::format("({} {})", Token::type_to_string(expr.op), expr_to_string(*expr.expr, source));
                          },
                          [source](const BinaryExpr &expr) {
                              return std::format("({} {} {})", Token::type_to_string(expr.op),
                                                 expr_to_string(*expr.left, source), expr_to_string(*expr.right, source));
                          },
                          [](const StringLiteral &expr) {
                              return std::format("\"{}\"", expr.string);
                          },
                          [source](const VariableExpr &expr) {
                              return expr.identifier.get_lexeme(source);
                          },
                          [source](const AssigmentExpr &expr) {
                              return std::format("(assign {} {})", expr.identifier.get_lexeme(source), expr_to_string(*expr.expr, source));
                          },
                          [source](const CallExpr &expr) {
                              std::string arguments_string;
                              for (auto& arg : expr.arguments) {
                                  arguments_string += " ";
                                  arguments_string += expr_to_string(*arg, source);
                              }
                              return std::format("(call {}{})", expr_to_string(*expr.callee, source), arguments_string);
                          },
                          [source](const GetPropertyExpr &expr) {
                              return std::format("({}.{})", expr_to_string(*expr.left, source), expr.property.get_lexeme(source));
                          },
                          [source](const SetPropertyExpr &expr) {
                              return std::format("(assign {}.{} {})",
                                  expr_to_string(*expr.left, source),
                                  expr.property.get_lexeme(source),
                                  expr_to_string(*expr.expression, source));
                          },
                          [source](const SuperExpr &expr) {
                              return std::format("super.{}", expr.method.get_lexeme(source));
                          },
                      }, expr);
}

#endif //EXPR_H
