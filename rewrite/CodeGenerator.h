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
    void begin_scope();

    void end_scope();

    void while_statement(const WhileStmt & stmt);
    void block_statement(const BlockStmt & stmt);
    void assigment(const AssigmentExpr & expr);
    void expr_statement(const ExprStmt & expr);
    void if_statement(const IfStmt & stmt);
    void variable(const VariableExpr& expr);
    void var_declaration(const VarStmt & expr);
    void literal(const LiteralExpr &expr);

    void visit_expr(const Expr &expr);

    void visit_stmt(const Stmt &stmt);

    void unary(const UnaryExpr & expr);
    void binary(const BinaryExpr & expr);
    void string_literal(const StringLiteral& expr);

    int start_jump(OpCode code);
    void patch_jump(int instruction_pos);

    int current_depth = 0;
    std::vector<std::pair<std::string, int>> locals;
    Module module;
    std::string_view source; // temp
};



#endif //CODEGENERATOR_H
