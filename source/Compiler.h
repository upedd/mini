#ifndef COMPILER_H
#define COMPILER_H
#include "Ast.h"
#include "Object.h"
#include "Parser.h"


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

        void close(int local);

        std::vector<Local> &get_locals();

    private:
        std::vector<Local> locals;
    };

    enum class FunctionType {
        FUNCTION,
        CONSTRUCTOR,
        METHOD
    };

    struct Context {
        Function *function = nullptr;
        FunctionType function_type;
        int current_depth = 0;
        Locals locals;
        std::vector<Upvalue> upvalues;

        int add_upvalue(int index, bool is_local);
    };

    explicit Compiler(std::string_view source) : parser(source), source(source), main("", 0) {
        context_stack.emplace_back(&main, FunctionType::FUNCTION);
        functions.push_back(&main);
    }

    void compile();

    Function& get_main();

    const std::vector<Function*>& get_functions();
    const std::vector<std::string>& get_natives();

    void native_declaration(const NativeStmt& stmt);

    void block(const BlockExpr & expr);

    void loop_expression(const LoopExpr & expr);

    void break_expr(const BreakExpr & expr);

private:
    void start_context(Function *function, FunctionType type);
    void end_context();
    Context &current_context();

    int &current_depth();
    [[nodiscard]] Function *current_function();
    Locals &current_locals();
    [[nodiscard]] Program &current_program();

    void emit(bite_byte byte);
    void emit(OpCode op_code);
    void emit(OpCode op_code, bite_byte value);
    void emit_default_return();

    void begin_scope();
    void end_scope();

    void define_variable(const std::string &name);
    void resolve_variable(const std::string &name);
    int resolve_upvalue(const std::string &name);

    void visit_stmt(const Stmt &stmt);

    void variable_declaration(const VarStmt &expr);
    void function_declaration(const FunctionStmt &stmt);
    void function(const FunctionStmt &stmt, FunctionType type);
    void class_declaration(const ClassStmt &stmt);
    void expr_statement(const ExprStmt &stmt);
    void return_statement(const ReturnStmt &stmt);
    void while_statement(const WhileStmt &stmt);

    void if_expression(const IfExpr &stmt);

    void visit_expr(const Expr &expr);

    void literal(const LiteralExpr &expr);
    void string_literal(const StringLiteral &expr);
    void unary(const UnaryExpr &expr);

    void binary(const BinaryExpr &expr);

    void update_lvalue(const Expr &lvalue);

    void logical(const BinaryExpr &expr);

    void variable(const VariableExpr& expr);
    void call(const CallExpr &expr);
    void get_property(const GetPropertyExpr & expr);
    void super(const SuperExpr& expr);

    Parser parser;
    Function main;
    std::vector<Context> context_stack;
    std::string_view source;
    std::vector<Function*> functions;
    std::vector<std::string> natives;
};


#endif //COMPILER_H
