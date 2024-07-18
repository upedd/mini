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

    enum class BlockType {
        BLOCK,
        LOOP
    };

    /**
     * Blocks enclosing loops or labeled blocks expressions
     */
    struct Block {
        int break_jump_idx;
        int continue_jump_idx;
        std::string label;
        BlockType type;
    };

    /*
     * TODO: comments!
     */
    struct Context;
    struct LocalAllocator {
        //bool contains(const std::string& name);
        void add_local(std::string& name) {
            context->compiler->emit(OpCode::SET, new_register(name));
        }
        void get_local(std::string& name) {
            context->compiler->emit(OpCode::GET, get_register(name));
        }

        int get_register(std::string& name) {
            for (int i = 0; i < locals.size(); ++i) {
                if (locals[i] == name) return start + i;
            }

        }
        int new_register(std::string& name) {
            locals.push_back(name);
            return start + locals.size() - 1;
        }
        [[nodiscard]] int allocated_count() const {
            return locals.size();
        }
        explicit LocalAllocator(Context* context) : context(context) {
            start = context->current_local_allocator().start + context->current_local_allocator().allocated_count();
            context->local_allocators_stack.push_back(this);

        }

        ~LocalAllocator() {
            context->local_allocators_stack.pop_back();
        }
        std::vector<std::string> locals;
        Context* context;
        LocalAllocator* outer;
        int start;

    };
    struct Context {
        Function *function = nullptr;
        FunctionType function_type;
        int current_depth = 0;
        Locals locals;
        std::vector<Upvalue> upvalues;
        std::vector<LocalAllocator*> local_allocators_stack;
        std::vector<Block> blocks;

        [[nodiscard]] LocalAllocator& current_local_allocator() const {
            return *local_allocators_stack.back();
        }
        int add_upvalue(int index, bool is_local);
        Compiler* compiler;
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

    void continue_expr(const ContinueExpr &expr);

    void while_expr(const WhileExpr & expr);

    void for_expr(const ForExpr & expr);

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
    void retrun_expression(const ReturnExpr &stmt);

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
