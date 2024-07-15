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
struct CallExpr;
struct LiteralExpr;
struct StringLiteral;
struct VariableExpr;
struct GetPropertyExpr;
struct SuperExpr;
struct BlockExpr;
struct IfExpr;
struct LoopExpr;
struct BreakExpr;
struct ContinueExpr;
struct WhileExpr;

struct ExprStmt;
struct VarStmt;
struct FunctionStmt;
struct ReturnStmt;
struct ClassStmt;
struct NativeStmt;

using Expr = std::variant<LiteralExpr, StringLiteral, UnaryExpr, BinaryExpr, VariableExpr, CallExpr, GetPropertyExpr,
    SuperExpr, BlockExpr, IfExpr, LoopExpr, BreakExpr, ContinueExpr, WhileExpr>;
using ExprHandle = std::unique_ptr<Expr>;

using Stmt = std::variant<VarStmt, ExprStmt, FunctionStmt, ReturnStmt, ClassStmt, NativeStmt>;
using StmtHandle = std::unique_ptr<Stmt>;

struct UnaryExpr {
    ExprHandle expr;
    Token::Type op;
};

struct BinaryExpr {
    ExprHandle left;
    ExprHandle right;
    Token::Type op;
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

struct SuperExpr {
    Token method;
};

struct BlockExpr {
    std::vector<StmtHandle> stmts;
    ExprHandle expr;
};

struct IfExpr {
    ExprHandle condition;
    ExprHandle then_expr;
    ExprHandle else_expr;
};

struct LoopExpr {
    ExprHandle body;
};

struct WhileExpr {
    ExprHandle condition;
    ExprHandle body;
};

struct BreakExpr {
    ExprHandle expr;
};
struct ContinueExpr {};

inline ExprHandle make_expr_handle(Expr expr) {
    return std::make_unique<Expr>(std::move(expr));
}

struct FunctionStmt {
    Token name;
    std::vector<Token> params;
    ExprHandle body;
};

struct VarStmt {
    Token name;
    ExprHandle value;
};

struct ExprStmt {
    ExprHandle expr;
};

struct ReturnStmt {
    ExprHandle expr;
};

struct ClassStmt {
    Token name;
    std::vector<std::unique_ptr<FunctionStmt> > methods;
    std::optional<Token> super_name;
};

struct NativeStmt {
    Token name;
};

inline std::string expr_to_string(const Expr &expr, std::string_view source);

inline std::string stmt_to_string(const Stmt &stmt, std::string_view source) {
    return std::visit(overloaded{
                          [source](const VarStmt &stmt) {
                              return std::format("(define {} {})", stmt.name.get_lexeme(source),
                                                 expr_to_string(*stmt.value, source));
                          },
                          [source](const ExprStmt &stmt) {
                              return expr_to_string(*stmt.expr, source);
                          },
                          [source](const FunctionStmt &stmt) {
                              std::string parameters_string;
                              for (auto &param: stmt.params) {
                                  parameters_string += param.get_lexeme(source);
                                  if (param.get_lexeme(source) != stmt.params.back().get_lexeme(source)) {
                                      parameters_string += " ";
                                  }
                              }
                              return std::format("(fun {} ({}) {})",
                                                 stmt.name.get_lexeme(source),
                                                 parameters_string,
                                                 expr_to_string(*stmt.body, source));
                          },
                          [source](const ReturnStmt &stmt) {
                              if (stmt.expr != nullptr) {
                                  return std::format("(return {})", expr_to_string(*stmt.expr, source));
                              }
                              return std::string("retrun");
                          },
                          [source](const ClassStmt &stmt) {
                              return std::format("(class {})", stmt.name.get_lexeme(source));
                          },
                          [source](const NativeStmt &stmt) {
                              return std::format("(native {})", stmt.name.get_lexeme(source));
                          }

                      }, stmt);
}

inline std::string expr_to_string(const Expr &expr, std::string_view source) {
    return std::visit(overloaded{
                          [](const LiteralExpr &expr) {
                              return expr.literal.to_string();
                          },
                          [source](const UnaryExpr &expr) {
                              return std::format("({} {})", Token::type_to_string(expr.op),
                                                 expr_to_string(*expr.expr, source));
                          },
                          [source](const BinaryExpr &expr) {
                              return std::format("({} {} {})", Token::type_to_string(expr.op),
                                                 expr_to_string(*expr.left, source),
                                                 expr_to_string(*expr.right, source));
                          },
                          [](const StringLiteral &expr) {
                              return std::format("\"{}\"", expr.string);
                          },
                          [source](const VariableExpr &expr) {
                              return expr.identifier.get_lexeme(source);
                          },
                          [source](const CallExpr &expr) {
                              std::string arguments_string;
                              for (auto &arg: expr.arguments) {
                                  arguments_string += " ";
                                  arguments_string += expr_to_string(*arg, source);
                              }
                              return std::format("(call {}{})", expr_to_string(*expr.callee, source), arguments_string);
                          },
                          [source](const GetPropertyExpr &expr) {
                              return std::format("({}.{})", expr_to_string(*expr.left, source),
                                                 expr.property.get_lexeme(source));
                          },
                          [source](const SuperExpr &expr) {
                              return std::format("super.{}", expr.method.get_lexeme(source));
                          },
                          [source](const BlockExpr &stmt) {
                              std::string blocks;
                              for (auto &s: stmt.stmts) {
                                  blocks += stmt_to_string(*s, source);
                                  if (s != stmt.stmts.back()) blocks += " ";
                              }
                              return std::format("({})", blocks);
                          },
                          [source](const IfExpr &expr) {
                              if (expr.else_expr != nullptr) {
                                  return std::format("(if {} {} {})",
                                                     expr_to_string(*expr.condition, source),
                                                     expr_to_string(*expr.then_expr, source),
                                                     expr_to_string(*expr.else_expr, source));
                              }
                              return std::format("(if {} {})",
                                                 expr_to_string(*expr.condition, source),
                                                 expr_to_string(*expr.then_expr, source));
                          },
                          [source](const LoopExpr& expr) {
                              return std::format("(loop {})", expr_to_string(*expr.body, source));
                          },
                          [](const BreakExpr& expr) {
                              return std::string("break");
                          },
                          [](const ContinueExpr& expr) {
                              return std::string("continue");
                          },
                          [source](const WhileExpr &expr) {
                              return std::format("(while {} {})",
                                                 expr_to_string(*expr.condition, source),
                                                 expr_to_string(*expr.body, source));
                          },
                      }, expr);
}

#endif //EXPR_H
