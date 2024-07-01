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

using Stmt = std::variant<VarStmt, ExprStmt>;
using StmtHandle = std::unique_ptr<Stmt>;

inline std::string stmt_to_string(const Stmt& stmt, std::string_view source) {
    return std::visit(overloaded {
        [source](const VarStmt& stmt) {return std::string("var ") + stmt.name.get_lexeme(source) + " = " + expr_to_string(*stmt.value);},
        [](const ExprStmt& stmt) {return expr_to_string(*stmt.expr);},
    }, stmt);
}

#endif //STMT_H
