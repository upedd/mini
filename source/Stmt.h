#ifndef STMT_H
#define STMT_H
#include <memory>
#include <variant>

#include "Expr.h"

// Reference: https://lesleylai.info/en/ast-in-cpp-part-1-variant/

struct ExprStmt;
struct VarStmt;
struct BlockStmt;
struct IfStmt;
struct WhileStmt;
struct FunctionStmt;
struct ReturnStmt;
struct ClassStmt;

using Stmt = std::variant<VarStmt, ExprStmt, BlockStmt, IfStmt, WhileStmt, FunctionStmt, ReturnStmt, ClassStmt>;
using StmtHandle = std::unique_ptr<Stmt>;

struct BlockStmt {
    std::vector<StmtHandle> stmts;
};

struct IfStmt {
    ExprHandle condition;
    StmtHandle then_stmt;
    StmtHandle else_stmt;
};

struct WhileStmt {
    ExprHandle condition;
    StmtHandle stmt;
};

struct FunctionStmt {
    Token name;
    std::vector<Token> params;
    StmtHandle body;
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
    std::vector<std::unique_ptr<FunctionStmt>> methods;
    std::optional<Token> super_name;
};

inline std::string stmt_to_string(const Stmt& stmt, std::string_view source) {
    return std::visit(overloaded {
        [source](const VarStmt& stmt) {
            return std::format("(define {} {})", stmt.name.get_lexeme(source), expr_to_string(*stmt.value, source));
        },
        [source](const ExprStmt& stmt) {
            return expr_to_string(*stmt.expr, source);
        },
        [source](const BlockStmt& stmt) {
                std::string blocks;
                for (auto& s : stmt.stmts) {
                    blocks += stmt_to_string(*s, source);
                    if (s != stmt.stmts.back()) blocks += " ";
                }
                return std::format("({})", blocks);
            },
        [source](const IfStmt& stmt) {
            if (stmt.else_stmt != nullptr) {
                return std::format("(if {} {} {})",
                    expr_to_string(*stmt.condition, source),
                    stmt_to_string(*stmt.then_stmt, source),
                    stmt_to_string(*stmt.else_stmt, source));
            }
            return std::format("(if {} {})",
                    expr_to_string(*stmt.condition, source),
                    stmt_to_string(*stmt.then_stmt, source));
        },
        [source](const WhileStmt& stmt) {
            return std::format("(while {} {})",
                expr_to_string(*stmt.condition, source),
                stmt_to_string(*stmt.stmt, source));
        },
        [source](const FunctionStmt& stmt) {
            std::string parameters_string;
            for (auto& param : stmt.params) {
                parameters_string += param.get_lexeme(source);
                if (param.get_lexeme(source) != stmt.params.back().get_lexeme(source)) {
                   parameters_string += " ";
                }
            }
            return std::format("(fun {} ({}) {})",
                stmt.name.get_lexeme(source),
                parameters_string,
                stmt_to_string(*stmt.body, source));
        },
        [source](const ReturnStmt& stmt) {
            if (stmt.expr != nullptr) {
                return std::format("(return {})", expr_to_string(*stmt.expr, source));
            }
            return std::string("retrun");
        },
        [source](const ClassStmt& stmt) {
            return std::format("(class {})", stmt.name.get_lexeme(source));
        }

    }, stmt);
}

#endif //STMT_H
