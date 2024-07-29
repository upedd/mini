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
struct ForExpr;
struct ReturnExpr;
struct ThisExpr;
struct ObjectExpr;

struct ExprStmt;
struct VarStmt;
struct FunctionStmt;
struct ClassStmt;
struct NativeStmt;
struct FieldStmt;
struct MethodStmt;
struct ConstructorStmt;
struct ObjectStmt;
struct TraitStmt;
struct UsingStmt;

using Expr = std::variant<LiteralExpr, StringLiteral, UnaryExpr, BinaryExpr, VariableExpr, CallExpr, GetPropertyExpr,
    SuperExpr, BlockExpr, IfExpr, LoopExpr, BreakExpr, ContinueExpr, WhileExpr, ForExpr, ReturnExpr, ThisExpr,
    ObjectExpr>;
using ExprHandle = std::unique_ptr<Expr>;

using Stmt = std::variant<VarStmt, ExprStmt, FunctionStmt, ClassStmt, NativeStmt, FieldStmt, MethodStmt,
    ConstructorStmt, ObjectStmt, TraitStmt, UsingStmt>;
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
    std::optional<Token> label;
};

struct IfExpr {
    ExprHandle condition;
    ExprHandle then_expr;
    ExprHandle else_expr;
};

struct LoopExpr {
    ExprHandle body;
    std::optional<Token> label;
};

struct WhileExpr {
    ExprHandle condition;
    ExprHandle body;
    std::optional<Token> label;
};

struct BreakExpr {
    ExprHandle expr;
    std::optional<Token> label;
};

struct ContinueExpr {
    std::optional<Token> label;
};

struct ForExpr {
    Token name;
    ExprHandle iterable;
    ExprHandle body;
    std::optional<Token> label;
};

struct ReturnExpr {
    ExprHandle value;
};


struct ThisExpr {
};

struct ObjectExpr {
    std::vector<std::unique_ptr<MethodStmt> > methods;
    std::vector<std::unique_ptr<FieldStmt> > fields;
    std::optional<Token> super_class;
    std::vector<ExprHandle> superclass_arguments;
    std::vector<std::unique_ptr<UsingStmt>> using_stmts;
};

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

struct FieldStmt {
    std::unique_ptr<VarStmt> variable;
    bitflags<ClassAttributes> attributes;
};

struct MethodStmt {
    std::unique_ptr<FunctionStmt> function;
    bitflags<ClassAttributes> attributes;
};

struct ClassStmt {
    Token name;
    std::unique_ptr<ConstructorStmt> constructor;
    std::vector<std::unique_ptr<MethodStmt>> methods;
    std::vector<std::unique_ptr<FieldStmt>> fields;
    ExprHandle class_object;
    std::optional<Token> super_class;
    bool is_abstract = false;
    std::vector<std::unique_ptr<UsingStmt>> using_statements;
};

struct ConstructorStmt {
    std::vector<Token> parameters;
    bool has_super;
    std::vector<ExprHandle> super_arguments;
    ExprHandle body;
};

struct NativeStmt {
    Token name;
};

struct ObjectStmt {
    Token name;
    ExprHandle object;
};

struct TraitStmt {
    Token name;
    std::vector<std::unique_ptr<MethodStmt>> methods;
    std::vector<std::unique_ptr<FieldStmt>> fields;
    std::vector<std::unique_ptr<UsingStmt>> using_stmts;
};

struct UsingStmtItem {
    Token name;
    std::vector<Token> exclusions;
    std::vector<std::pair<Token, Token>> aliases;
};

struct UsingStmt {
    std::vector<UsingStmtItem> items;
};

inline std::string expr_to_string(const Expr &expr, std::string_view source);

inline std::string stmt_to_string(const Stmt &stmt, std::string_view source) {
    return std::visit(overloaded{
                          [source](const VarStmt &stmt) {
                              return std::format("(define {} {})", *stmt.name.string,
                                                 expr_to_string(*stmt.value, source));
                          },
                          [source](const ExprStmt &stmt) {
                              return expr_to_string(*stmt.expr, source);
                          },
                          [source](const FunctionStmt &stmt) {
                              std::string parameters_string;
                              for (auto &param: stmt.params) {
                                  parameters_string += *param.string;
                                  if (*param.string != *stmt.params.back().string) {
                                      parameters_string += " ";
                                  }
                              }
                              return std::format("(fun {} ({}) {})",
                                                 *stmt.name.string,
                                                 parameters_string,
                                                 expr_to_string(*stmt.body, source));
                          },

                          [source](const ClassStmt &stmt) {
                              return std::format("(class {})", *stmt.name.string);
                          },
                          [source](const NativeStmt &stmt) {
                              return std::format("(native {})", *stmt.name.string);
                          },
                          [source](const MethodStmt &) {
                              return std::string("method");
                          },
                          [source](const FieldStmt &) {
                              return std::string("field");
                          },
                          [](const ConstructorStmt &stmt) {
                              return std::string("constructor"); // TODO: implement!
                          },
                          [](const ObjectStmt &stmt) {
                              return std::string("object"); // TODO: implement!
                          },
                          [](const TraitStmt& stmt) {
                              return std::string("trait"); // TODO: implement!
                          },
                          [](const UsingStmt& stmt) {
                              return std::string("using"); // TODO: implement!
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
                              return *expr.identifier.string;
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
                                                 *expr.property.string);
                          },
                          [source](const SuperExpr &expr) {
                              return std::format("super.{}", *expr.method.string);
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
                          [source](const LoopExpr &expr) {
                              return std::format("(loop {})", expr_to_string(*expr.body, source));
                          },
                          [](const BreakExpr &expr) {
                              return std::string("break");
                          },
                          [](const ContinueExpr &expr) {
                              return std::string("continue");
                          },
                          [source](const WhileExpr &expr) {
                              return std::format("(while {} {})",
                                                 expr_to_string(*expr.condition, source),
                                                 expr_to_string(*expr.body, source));
                          },
                          [source](const ForExpr &expr) {
                              return std::string("for"); // TODO: implement
                          },
                          [source](const ReturnExpr &stmt) {
                              if (stmt.value != nullptr) {
                                  return std::format("(return {})", expr_to_string(*stmt.value, source));
                              }
                              return std::string("retrun");
                          },
                          [](const ThisExpr &expr) {
                              return std::string("this");
                          },
                          [](const ObjectExpr &expr) {
                              return std::string("object"); // TODO: implement!
                          },
                      }, expr);
}

#endif //EXPR_H
