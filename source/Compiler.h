#ifndef COMPILER_H
#define COMPILER_H
#include "Expr.h"
#include "Function.h"
#include "Parser.h"
#include "Stmt.h"


class Compiler {
public:
    class Error : public std::runtime_error {
    public:
        explicit Error(const std::string &message) : runtime_error(message) {
        }
    };

    struct Upvalue {
        int index; // should be other type
        bool is_local;

        bool operator==(const Upvalue & upvalue) const = default;
    };

    struct Local {
        std::string name;
        int depth;
        bool is_closed = false;
    };

    class Locals {
    public:
        [[nodiscard]] int get(const std::string &name) const;

        [[nodiscard]] bool contains(const std::string &name, int depth) const;

        bool define(const std::string &name, int depth);

        // returns number of elements that have been erased
        int erase_above(int depth);

        void close(int local);

        std::vector<Compiler::Local> &get_locals();

    private:
        std::vector<Local> locals;
    };

    class JumpDestination {
    public:
        explicit JumpDestination(const int position) : position(position) {}

        void make_jump(Program& program) const;
    private:
        int position;
    };

    class JumpHandle {
    public:
        explicit JumpHandle( const int instruction_position) : instruction_position(instruction_position) {}

        void mark_destination(Program &program) const;
    private:
        int instruction_position;
    };

    struct State {
        Function *function = nullptr;
        int current_depth = 0;
        Locals locals;
        std::vector<Upvalue> upvalues;
    };

    explicit Compiler(std::string_view source) : parser(source), source(source), main("", 0) {
        allocated_objects.push_back(&main);
        states.emplace_back(&main);
    }

    void compile();

    Function &get_main() ;

    Function *get_function();

    void define_variable(const std::string &name);

    void function_declaration(const FunctionStmt &stmt);

    void call(const CallExpr &expr);

    void return_statement(const ReturnStmt &stmt);

    void class_declaration(const ClassStmt &stmt);

    void get_property(const GetPropertyExpr & expr);

    void set_property(const SetPropertyExpr & expr);

    void super(const SuperExpr& expr);

    std::vector<Object*> allocated_objects;

private:
    void emit(OpCode op_code);

    void emit(bite_byte byte);

    void begin_scope();

    void end_scope();

    Program &current_program() const;

    void while_statement(const WhileStmt &stmt);

    void block_statement(const BlockStmt &stmt);

    int add_upvalue(int index, bool is_local, int distance);

    int resolve_upvalue(const std::string &name, int distance);

    void assigment(const AssigmentExpr &expr);

    void expr_statement(const ExprStmt &expr);

    void if_statement(const IfStmt &stmt);

    void variable(const VariableExpr &expr);

    void variable_declaration(const VarStmt &expr);

    void literal(const LiteralExpr &expr);

    void visit_expr(const Expr &expr);

    void visit_stmt(const Stmt &stmt);

    void unary(const UnaryExpr &expr);

    void logical(const BinaryExpr &expr);

    void binary(const BinaryExpr &expr);

    void string_literal(const StringLiteral &expr);

    int &get_current_depth();

    Function *current_function() const;

    Locals &current_locals();

    [[nodiscard]] JumpDestination mark_destination() const;
    JumpHandle start_jump(OpCode op_code);

    Parser parser;
    Function main;
    std::vector<State> states;
    std::string_view source;
};


#endif //COMPILER_H
