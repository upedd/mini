#ifndef STMT_H
#define STMT_H
#include <memory>
#include <variant>

#include "Expr.h"

// Reference: https://lesleylai.info/en/ast-in-cpp-part-1-variant/

struct VarStmt {
    Token name;
    ExprHandle value;
};

struct ExprStmt {
    ExprHandle expr;
};

struct BlockStmt;

using Stmt = std::variant<VarStmt, ExprStmt, BlockStmt>;
using StmtHandle = std::unique_ptr<Stmt>;

struct BlockStmt {
    std::vector<StmtHandle> stmts;
};

inline std::string stmt_to_string(const Stmt& stmt, std::string_view source) {
    return std::visit(overloaded {
        [source](const VarStmt& stmt) {return std::string("var ") + stmt.name.get_lexeme(source) + " = " + expr_to_string(*stmt.value);},
        [](const ExprStmt& stmt) {return expr_to_string(*stmt.expr);},
        [source](const BlockStmt& stmt) {
                std::string out = "block: ";
                for (auto& st : stmt.stmts) {
                    out += stmt_to_string(*st, source);
                    out += ", ";
                }
                return out;
            }
    }, stmt);
}

#endif //STMT_H
