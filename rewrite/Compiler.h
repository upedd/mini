#ifndef COMPILER_H
#define COMPILER_H
#include "Expr.h"
#include "Function.h"
#include "Module.h"
#include "Parser.h"
#include "Stmt.h"


class Compiler {
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

    explicit Compiler(std::string_view source) : parser(source), source(source), main("", 0) {
        states.emplace_back(&main);
    }

    void compile();

    [[nodiscard]] const Function& get_main() const;

    Module get_module();
    Function* get_function();

    void define_variable(const std::string &name);

    void function_declration(const FunctionStmt & stmt);

    void call(const CallExpr& expr);

    void return_statement(const ReturnStmt & stmt);

private:
    void emit(OpCode op_code);
    void emit(bite_byte byte);

    void begin_scope();

    void end_scope();

    Program& current_program();

    void while_statement(const WhileStmt & stmt);
    void block_statement(const BlockStmt & stmt);
    void assigment(const AssigmentExpr & expr);
    void expr_statement(const ExprStmt & expr);
    void if_statement(const IfStmt & stmt);
    void variable(const VariableExpr& expr);
    void variable_declaration(const VarStmt & expr);
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

    Function* current_function() const;
    std::vector<std::pair<std::string, int>> &get_current_locals();

    Parser parser;
    Function main;
    std::vector<State> states;
    std::string_view source;
};



#endif //COMPILER_H
