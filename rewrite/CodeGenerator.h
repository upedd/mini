#ifndef CODEGENERATOR_H
#define CODEGENERATOR_H
#include <unordered_map>

#include "Expr.h"
#include "Module.h"
#include "Stmt.h"


class CodeGenerator {
public:
    class Error : public std::runtime_error {
    public:
        explicit Error(const std::string& message) : runtime_error(message) {}
    };

    void generate(const std::vector<Stmt>& stmts, std::string_view source);

    Module get_module();
private:
    void assigment(const AssigmentExpr & expr);
    void expr_statement(const ExprStmt & expr);

    void variable(const VariableExpr& expr);
    void var_declaration(const VarStmt & expr);
    void literal(const LiteralExpr &expr);

    void visit_expr(const Expr &expr);

    void visit_stmt(const Stmt &stmt);

    void unary(const UnaryExpr & expr);
    void binary(const BinaryExpr & expr);
    void string_literal(const StringLiteral& expr);

    std::vector<std::string> locals;
    Module module;
    std::string_view source; // temp
};



#endif //CODEGENERATOR_H
