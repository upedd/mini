#ifndef EXPR_H
#define EXPR_H
#include <format>
#include <variant>
#include <vector>

#include "parser/Token.h"
#include "Value.h"
#include "base/box.h"

// Reference: https://lesleylai.info/en/ast-in-cpp-part-1-variant/
// https://www.foonathan.net/2022/05/recursive-variant-box/
using Expr = std::variant<bite::box<struct LiteralExpr>, bite::box<struct StringLiteral>, bite::box<struct UnaryExpr>,
                          bite::box<struct BinaryExpr>, bite::box<struct VariableExpr>, bite::box<struct CallExpr>,
                          bite::box<struct GetPropertyExpr>, bite::box<struct SuperExpr>, bite::box<struct BlockExpr>,
                          bite::box<struct IfExpr>, bite::box<struct LoopExpr>, bite::box<struct BreakExpr>, bite::box<
                              struct ContinueExpr>, bite::box<struct WhileExpr>, bite::box<struct ForExpr>, bite::box<
                              struct ReturnExpr>, bite::box<struct ThisExpr>, bite::box<struct ObjectExpr>, bite::box<struct InvalidExpr>>;

using Stmt = std::variant<bite::box<struct VarStmt>, bite::box<struct ExprStmt>, bite::box<struct FunctionStmt>,
                          bite::box<struct ClassStmt>, bite::box<struct NativeStmt>, bite::box<struct FieldStmt>,
                          bite::box<struct MethodStmt>, bite::box<struct ConstructorStmt>, bite::box<struct ObjectStmt>,
                          bite::box<struct TraitStmt>, bite::box<struct UsingStmt>, bite::box<struct InvalidStmt>>;

struct Ast {
    std::vector<Stmt> statements;
};

struct UnaryExpr {
    Expr expr;
    Token::Type op;
};

struct BinaryExpr {
    Expr left;
    Expr right;
    Token::Type op;
};

struct CallExpr {
    Expr callee;
    std::vector<Expr> arguments;
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
    Expr left; // Todo: better name?
    Token property;
};

struct SuperExpr {
    Token method;
};

struct BlockExpr {
    std::vector<Stmt> stmts;
    std::optional<Expr> expr;
    std::optional<Token> label;
};

struct IfExpr {
    Expr condition;
    Expr then_expr;
    std::optional<Expr> else_expr;
};

struct LoopExpr {
    Expr body;
    std::optional<Token> label;
};

struct WhileExpr {
    Expr condition;
    Expr body;
    std::optional<Token> label;
};

struct BreakExpr {
    std::optional<Expr> expr;
    std::optional<Token> label;
};

struct ContinueExpr {
    std::optional<Token> label;
};

struct ForExpr {
    Token name;
    Expr iterable;
    Expr body;
    std::optional<Token> label;
};

struct ReturnExpr {
    std::optional<Expr> value;
};


struct ThisExpr {};



struct FunctionStmt {
    Token name;
    std::vector<Token> params;
    std::optional<Expr> body;
};

struct VarStmt {
    Token name;
    std::optional<Expr> value;
};

struct ExprStmt {
    Expr expr;
};

struct FieldStmt {
    VarStmt variable;
    bitflags<ClassAttributes> attributes;
};

struct MethodStmt {
    FunctionStmt function;
    bitflags<ClassAttributes> attributes;
};

struct ConstructorStmt {
    std::vector<Token> parameters;
    bool has_super;
    std::vector<Expr> super_arguments;
    Expr body;
};

/**
 * Shared fields between ObjectExpr and ClassStmt
 */
struct StructureBody {
    std::vector<MethodStmt> methods;
    std::vector<FieldStmt> fields;
    std::optional<Expr> class_object;
    std::vector<UsingStmt> using_statements;
    std::optional<ConstructorStmt> constructor;
};

struct ObjectExpr {
    StructureBody body;
    std::optional<Token> super_class;
    std::vector<Expr> superclass_arguments;
};

struct ClassStmt {
    Token name;
    std::optional<Token> super_class;
    StructureBody body;
    bool is_abstract = false;
};

struct NativeStmt {
    Token name;
};

struct ObjectStmt {
    Token name;
    Expr object;
};

struct TraitStmt {
    Token name;
    std::vector<MethodStmt> methods;
    std::vector<FieldStmt> fields;
    std::vector<UsingStmt> using_stmts;
};

struct UsingStmtItem {
    Token name;
    std::vector<Token> exclusions;
    std::vector<std::pair<Token, Token>> aliases;
};

struct UsingStmt {
    std::vector<UsingStmtItem> items;
};

// markers for invalid ast nodes
struct InvalidExpr {};
struct InvalidStmt {};

// inline std::string expr_to_string(const Expr& expr, std::string_view source);
//
// inline std::string stmt_to_string(const Stmt& stmt, std::string_view source) {
//     return std::visit(
//         overloaded {
//             [source](const VarStmt& stmt) {
//                 return std::format("(define {} {})", *stmt.name.string, expr_to_string(*stmt.value, source));
//             },
//             [source](const ExprStmt& stmt) {
//                 return expr_to_string(*stmt.expr, source);
//             },
//             [source](const FunctionStmt& stmt) {
//                 std::string parameters_string;
//                 for (auto& param : stmt.params) {
//                     parameters_string += *param.string;
//                     if (*param.string != *stmt.params.back().string) {
//                         parameters_string += " ";
//                     }
//                 }
//                 return std::format(
//                     "(fun {} ({}) {})",
//                     *stmt.name.string,
//                     parameters_string,
//                     expr_to_string(*stmt.body, source)
//                 );
//             },
//             [source](const ClassStmt& stmt) {
//                 return std::format("(class {})", *stmt.name.string);
//             },
//             [source](const NativeStmt& stmt) {
//                 return std::format("(native {})", *stmt.name.string);
//             },
//             [source](const MethodStmt&) {
//                 return std::string("method");
//             },
//             [source](const FieldStmt&) {
//                 return std::string("field");
//             },
//             [](const ConstructorStmt& stmt) {
//                 return std::string("constructor"); // TODO: implement!
//             },
//             [](const ObjectStmt& stmt) {
//                 return std::string("object"); // TODO: implement!
//             },
//             [](const TraitStmt& stmt) {
//                 return std::string("trait"); // TODO: implement!
//             },
//             [](const UsingStmt& stmt) {
//                 return std::string("using"); // TODO: implement!
//             }
//         },
//         stmt
//     );
// }
//
// inline std::string expr_to_string(const Expr& expr, std::string_view source) {
//     return std::visit(
//         overloaded {
//             [](const LiteralExpr& expr) {
//                 return expr.literal.to_string();
//             },
//             [source](const UnaryExpr& expr) {
//                 return std::format("({} {})", Token::type_to_string(expr.op), expr_to_string(*expr.expr, source));
//             },
//             [source](const BinaryExpr& expr) {
//                 return std::format(
//                     "({} {} {})",
//                     Token::type_to_string(expr.op),
//                     expr_to_string(*expr.left, source),
//                     expr_to_string(*expr.right, source)
//                 );
//             },
//             [](const StringLiteral& expr) {
//                 return std::format("\"{}\"", expr.string);
//             },
//             [source](const VariableExpr& expr) {
//                 return *expr.identifier.string;
//             },
//             [source](const CallExpr& expr) {
//                 std::string arguments_string;
//                 for (auto& arg : expr.arguments) {
//                     arguments_string += " ";
//                     arguments_string += expr_to_string(*arg, source);
//                 }
//                 return std::format("(call {}{})", expr_to_string(*expr.callee, source), arguments_string);
//             },
//             [source](const GetPropertyExpr& expr) {
//                 return std::format("({}.{})", expr_to_string(*expr.left, source), *expr.property.string);
//             },
//             [source](const SuperExpr& expr) {
//                 return std::format("super.{}", *expr.method.string);
//             },
//             [source](const BlockExpr& stmt) {
//                 std::string blocks;
//                 for (auto& s : stmt.stmts) {
//                     blocks += stmt_to_string(*s, source);
//                     if (s != stmt.stmts.back())
//                         blocks += " ";
//                 }
//                 return std::format("({})", blocks);
//             },
//             [source](const IfExpr& expr) {
//                 if (expr.else_expr != nullptr) {
//                     return std::format(
//                         "(if {} {} {})",
//                         expr_to_string(*expr.condition, source),
//                         expr_to_string(*expr.then_expr, source),
//                         expr_to_string(*expr.else_expr, source)
//                     );
//                 }
//                 return std::format(
//                     "(if {} {})",
//                     expr_to_string(*expr.condition, source),
//                     expr_to_string(*expr.then_expr, source)
//                 );
//             },
//             [source](const LoopExpr& expr) {
//                 return std::format("(loop {})", expr_to_string(*expr.body, source));
//             },
//             [](const BreakExpr& expr) {
//                 return std::string("break");
//             },
//             [](const ContinueExpr& expr) {
//                 return std::string("continue");
//             },
//             [source](const WhileExpr& expr) {
//                 return std::format(
//                     "(while {} {})",
//                     expr_to_string(*expr.condition, source),
//                     expr_to_string(*expr.body, source)
//                 );
//             },
//             [source](const ForExpr& expr) {
//                 return std::string("for"); // TODO: implement
//             },
//             [source](const ReturnExpr& stmt) {
//                 if (stmt.value != nullptr) {
//                     return std::format("(return {})", expr_to_string(*stmt.value, source));
//                 }
//                 return std::string("retrun");
//             },
//             [](const ThisExpr& expr) {
//                 return std::string("this");
//             },
//             [](const ObjectExpr& expr) {
//                 return std::string("object"); // TODO: implement!
//             },
//         },
//         expr
//     );
// }

#endif //EXPR_H
