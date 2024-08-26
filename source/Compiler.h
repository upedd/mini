#ifndef COMPILER_H
#define COMPILER_H

#include "Analyzer.h"
#include "Ast.h"
#include "Object.h"
#include "base/overloaded.h"
#include "parser/Parser.h"


class Compiler : AstVisitor {
public:
    class Error : public std::runtime_error {
    public:
        explicit Error(const std::string& message) : runtime_error(message) {}
    };

    enum class FunctionType : std::uint8_t {
        FUNCTION,
        CONSTRUCTOR,
        METHOD
    };

    struct FieldInfo {
        bitflags<ClassAttributes> attributes;
    };

    // TODO: refactor?
    struct BlockScope {
        std::int64_t on_stack_before = -1;
        std::int64_t return_slot = -1;
    };

    struct LabeledBlockScope {
        std::int64_t on_stack_before = -1;
        std::int64_t return_slot = -1;
        StringTable::Handle label;
        int break_idx = -1;
    };

    struct LoopScope {
        std::int64_t on_stack_before = -1;
        std::int64_t return_slot = -1;
        std::optional<StringTable::Handle> label;
        int break_idx = -1;
        int continue_idx = -1;
    };

    using ExpressionScope = std::variant<BlockScope, LabeledBlockScope, LoopScope>;

    struct Slot {
        int64_t index;
        bool is_captured;
    };

    struct Context {
        Function* function = nullptr;
        FunctionType function_type;
        std::vector<Upvalue> upvalues;
        std::int64_t on_stack = 0;
        // enviroment offset to slot
        bite::unordered_dense::map<std::uint64_t, Slot> slots;
        std::vector<ExpressionScope> expression_scopes;
        bite::unordered_dense::set<int64_t> open_upvalues_slots;
    };

    // perfomance?
    // concept?
    template <typename T>
    void with_expression_scope(T&& scope, const auto& fn) {
        begin_expression_scope(std::forward<T>(scope));
        fn(current_context().expression_scopes.back());
        end_expression_scope();
    }

    void with_context(Function* function, FunctionType type, const auto& fn) {
        start_context(function, type);
        fn();
        end_context();
    }

    void begin_expression_scope(ExpressionScope&& scope) {
        // refactor?
        emit(OpCode::NIL); // sensible default i guess?
        std::visit(
            overloaded {
                [this](BlockScope&& sc) {
                    // deduplicate!
                    sc.on_stack_before = current_context().on_stack;
                    sc.return_slot = current_context().on_stack;
                    current_context().expression_scopes.emplace_back(std::move(sc));
                },
                [this](LabeledBlockScope&& sc) {
                    // deduplicate!
                    sc.on_stack_before = current_context().on_stack;
                    sc.return_slot = current_context().on_stack;
                    current_context().expression_scopes.emplace_back(std::move(sc));
                },
                [this](LoopScope&& sc) {
                    // deduplicate!
                    sc.on_stack_before = current_context().on_stack;
                    sc.return_slot = current_context().on_stack;
                    current_context().expression_scopes.emplace_back(std::move(sc));
                }
            },
            std::move(scope)
        );
        current_context().on_stack++;
    }

    static std::int64_t get_on_stack_before(const ExpressionScope& scope) {
        return std::visit(
            overloaded {
                [](const auto& sc) {
                    return sc.on_stack_before;
                }
            },
            scope
        );
    }

    void end_expression_scope() {
        pop_out_of_scopes(1);
        current_context().expression_scopes.pop_back();
    }

    // TODO: do this better
    ExpressionScope& current_expression_scope() {
        return current_context().expression_scopes.back();
    }

    static bite_byte get_return_slot(const ExpressionScope& scope) {
        return std::visit(
            overloaded {
                [](const auto& sc) {
                    return sc.return_slot;
                }
            },
            scope
        );
    }


    explicit Compiler(bite::file_input_stream&& stream, SharedContext* context) : parser(std::move(stream), context),
        main("", 0),
        shared_context(context),
        analyzer(context) {
        context_stack.emplace_back(&main, FunctionType::FUNCTION);
        functions.push_back(&main);
    }

    bool compile();

    Function& get_main();

    const std::vector<Function*>& get_functions();
    const std::vector<std::string>& get_natives();

    void this_expr(const ThisExpr& expr);

    void object_expr(const ObjectExpr& expr);

    void object_declaration(const ObjectDeclaration& stmt);

    void trait_declaration(const TraitDeclaration& stmt);
    void class_object(const std::string& name, bool is_abstract, const ClassObject& object);
    void variable_declaration(const VariableDeclaration& expr);
    void function_declaration(const FunctionDeclaration& stmt);
    void function(const FunctionDeclaration& stmt, FunctionType type);

    void constructor(const Constructor& stmt, const std::vector<Field>& fields, bool has_superclass);

    void class_declaration(const ClassDeclaration& stmt);
    void expr_stmt(const ExprStmt& stmt);
    void native_declaration(const NativeDeclaration& stmt);

    void if_expr(const IfExpr& stmt);

    void block_expr(const BlockExpr& expr);
    void loop_expr(const LoopExpr& expr);
    void pop_out_of_scopes(int64_t depth);
    void break_expr(const BreakExpr& expr);
    void continue_expr(const ContinueExpr& expr);
    void while_expr(const WhileExpr& expr);
    void for_expr(const ForExpr& expr);
    void return_expr(const ReturnExpr& stmt);

    void literal_expr(const LiteralExpr& expr);
    void string_expr(const StringExpr& expr);
    void unary_expr(const UnaryExpr& expr);

    void binary_expr(const BinaryExpr& expr);

    void logical_expr(const BinaryExpr& expr);

    void variable_expr(const VariableExpr& expr);
    void call_expr(const CallExpr& expr);
    void get_property_expr(const GetPropertyExpr& expr);
    void super_expr(const SuperExpr& expr);

    void invalid_stmt(const InvalidStmt& /*unused*/) {}
    void invalid_expr(const InvalidExpr& /*unused*/) {}

private:
    void start_context(Function* function, FunctionType type);
    void end_context();
    Context& current_context();
    // Scope& current_scope();
    [[nodiscard]] Function* current_function();
    [[nodiscard]] Program& current_program();

    void emit(bite_byte byte);
    void emit(OpCode op_code);
    void emit(OpCode op_code, bite_byte value);
    void emit_default_return();

    void define_variable(const DeclarationInfo& info);
    int64_t synthetic_variable();


    void emit_set_variable(const Binding& binding);

    void emit_get_variable(const Binding& binding);

    Parser parser;
    Function main;
    std::vector<Context> context_stack;
    std::vector<Function*> functions;
    std::vector<std::string> natives;
    SharedContext* shared_context;
    bite::Analyzer analyzer;
    Ast ast;
};


#endif //COMPILER_H
