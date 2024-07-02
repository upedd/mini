#ifndef CODEGENERATOR_H
#define CODEGENERATOR_H
#include "Expr.h"
#include "Function.h"
#include "Module.h"
#include "Stmt.h"


class CodeGenerator {
public:
    class Error : public std::runtime_error {
    public:
        explicit Error(const std::string& message) : runtime_error(message) {}
    };
    struct State {
        Function* function = nullptr;
        int current_depth = 0;
        std::vector<std::pair<std::string, int>> locals;
    };

    CodeGenerator() {
        states.emplace_back(allocate_function());
    }

    void generate(const std::vector<Stmt>& stmts, std::string_view source);

    Module get_module();
    Function* get_function();

    void define_variable(const std::string &name);

    void function_statement(const FunctionStmt & stmt);

    void call(const CallExpr& expr);

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

    void logical(const BinaryExpr & expr);

    void binary(const BinaryExpr & expr);
    void string_literal(const StringLiteral& expr);

    int start_jump(OpCode code);
    void patch_jump(int instruction_pos);

    int &get_current_depth();
    Function* get_current_function() const;
    std::vector<std::pair<std::string, int>> get_current_locals();

    Module& current_module() const;

    std::vector<State> states;
    //Module module;
    std::string_view source; // temp
};



#endif //CODEGENERATOR_H
