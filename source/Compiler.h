#ifndef COMPILER_H
#define COMPILER_H
#include <unordered_set>

#include "Analyzer.h"
#include "Ast.h"
#include "Object.h"
#include "parser/Parser.h"


class Compiler {
public:
    class Error : public std::runtime_error {
    public:
        explicit Error(const std::string& message) : runtime_error(message) {}
    };

    // struct Upvalue {
    //     int index; // should be other type
    //     bool is_local;
    //
    //     bool operator==(const Upvalue& upvalue) const = default;
    // };
    //
    // struct Local {
    //     std::string name;
    //     bool is_closed = false;
    // };
    //
    enum class FunctionType {
        FUNCTION,
        CONSTRUCTOR,
        METHOD
    };

    // enum class ScopeType {
    //     LOOP,
    //     LABELED_BLOCK,
    //     BLOCK,
    //     CLASS
    // };

    struct FieldInfo {
        bitflags<ClassAttributes> attributes;
    };

    // Break this scope into classes because it is too monolithic
    // class Scope {
    // public:
    //     Scope(ScopeType type, int slot_start, std::string name = "") : type(type),
    //                                                                    name(std::move(name)),
    //                                                                    slot_start(slot_start) {}
    //
    //     void mark_temporary(int count = 1);
    //
    //     void pop_temporary(int count = 1);
    //
    //     int define(const std::string& name);
    //     std::optional<int> get(const std::string& name);
    //     void close(int index);
    //     int next_slot();
    //
    //     [[nodiscard]] int get_on_stack_count() const {
    //         return temporaries + locals.size();
    //     }
    //
    //     [[nodiscard]] int get_temporaries_count() const {
    //         return temporaries;
    //     }
    //
    //     [[nodiscard]] const std::string& get_name() const {
    //         return name;
    //     }
    //
    //     [[nodiscard]] ScopeType get_type() const {
    //         return type;
    //     }
    //
    //     [[nodiscard]] int get_start_slot() const {
    //         return slot_start;
    //     }
    //
    //     [[nodiscard]] const std::vector<Local>& get_locals() const {
    //         return locals;
    //     }
    //
    //     [[nodiscard]] bool has_field(const std::string& name) const {
    //         return fields.contains(name);
    //     }
    //
    //     [[nodiscard]] std::unordered_map<std::string, FieldInfo>& get_fields() {
    //         return fields;
    //     }
    //
    //     void add_field(const std::string& string, FieldInfo field_info) {
    //         fields[string] = field_info;
    //     }
    //
    //     FieldInfo get_field_info(const std::string& name) {
    //         return fields[name];
    //     }
    //
    //     int break_idx = -1;
    //     int continue_idx = -1;
    //     int return_slot = -1;
    //
    //     int constructor_argument_count = 0;
    //
    // private:
    //     ScopeType type;
    //     std::string name;
    //     int slot_start;
    //     int temporaries = 0;
    //
    //     std::vector<Local> locals;
    //     std::unordered_map<std::string, FieldInfo> fields;
    // };

    struct ResolvedClass {
        std::unordered_map<std::string, FieldInfo> fields;
        int constructor_argument_count = 0;
    };

    // struct ExpressionScope {
    //     std::int64_t on_stack_before;
    //     std::int64_t return_slot;
    //     std::optional<StringTable::Handle> label;
    //     // Maybe break into different
    //     int break_idx = -1;
    //     int continue_idx = -1;
    // };

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
        int index;
        bool is_captured;
    };

    struct Context {
        Function* function = nullptr;
        FunctionType function_type;
        std::vector<Upvalue> upvalues;
        //std::vector<Scope> scopes;
        std::unordered_map<std::string, ResolvedClass> resolved_classes;
        int on_stack = 0;
        // enviroment offset to slot
        bite::unordered_dense::map<std::uint64_t, Slot> slots;
        std::vector<ExpressionScope> expression_scopes;
        bite::unordered_dense::set<int64_t> open_upvalues_slots;

        // Scope& current_scope() {
        //     return scopes.back();
        // }

        // TODO: does this support nested classes??
        // struct FieldResolution {};
        //
        // struct LocalResolution {
        //     int slot;
        // };
        //
        // using Resolution = std::variant<std::monostate, FieldResolution, LocalResolution>;
        //
        // Resolution resolve_variable(const std::string& name);
        //
        // int add_upvalue(int index, bool is_local);
        //
        // void close_upvalue(int index);
    };

    // perfomance?
    // concept?
    template<typename T>
    void with_expression_scope(T&& scope, const auto& fn) {
        begin_expression_scope(std::forward<T>(scope));
        fn(current_context().expression_scopes.back());
        end_expression_scope();
    }

    void with_context(Function* function, FunctionType& type, const auto& fn) {
        start_context(function, type);
        fn();
        end_context();
    }

    void begin_expression_scope(ExpressionScope&& scope) {
        // refactor?
        emit(OpCode::NIL); // sensible default i guess?
        std::visit(overloaded {
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
        }, std::move(scope));
        current_context().on_stack++;
    }

    std::int64_t get_on_stack_before(const ExpressionScope& scope) {
        // refactor?
        return std::visit(overloaded {
            [](const BlockScope& sc) {
                // deduplicate!
                return sc.on_stack_before;
            },
            [](const LabeledBlockScope& sc) {
                // deduplicate!
                return sc.on_stack_before;
            },
            [](const LoopScope& sc) {
                // deduplicate!
                return sc.on_stack_before;
            }
        }, scope);
    }

    void end_expression_scope() {
        pop_out_of_scopes(1);
        current_context().expression_scopes.pop_back();
    }

    // TODO: do this better
    ExpressionScope& current_expression_scope() {
        return current_context().expression_scopes.back();
    }

    bite_byte get_return_slot(const ExpressionScope& scope) {
        // refactor?
        return std::visit(overloaded {
            [](const BlockScope& sc) {
                // deduplicate!
                return sc.return_slot;
            },
            [](const LabeledBlockScope& sc) {
                // deduplicate!
                return sc.return_slot;
            },
            [](const LoopScope& sc) {
                // deduplicate!
                return sc.return_slot;
            }
        }, scope);
    }


    explicit Compiler(bite::file_input_stream&& stream, SharedContext* context) : parser(std::move(stream), context), main("", 0),
                                                                                  shared_context(context),
                                                                                  analyzer(context) {
        context_stack.emplace_back(&main, FunctionType::FUNCTION);
        //current_context().scopes.emplace_back(ScopeType::BLOCK, 0); // TODO: special type?
        functions.push_back(&main);
    }

    bool compile();

    Function& get_main();

    const std::vector<Function*>& get_functions();
    const std::vector<std::string>& get_natives();

    void this_expr();

    void object_constructor(
        const std::vector<Field>& fields,
        bool has_superclass,
        const std::vector<Expr>& superclass_arguments
    );

    void object_expression(const AstNode<ObjectExpr>& expr);

    void object_statement(const AstNode<ObjectStmt>& stmt);

    void trait_statement(const AstNode<TraitStmt>& stmt);

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

    // void begin_scope(ScopeType type, const std::string& label = "");
    // void end_scope();
    // void pop_out_of_scopes(int depth);

    // void define_variable(const std::string& name);
    // void resolve_variable(const std::string& name);
    //
    // Compiler::Context::Resolution resolve_upvalue(const std::string& name);

    void visit_stmt(const Stmt& stmt);
    void define_variable(const bite::Analyzer::Binding& binding, int64_t declaration_idx);
    bite::Analyzer::Binding get_binding(const Expr& expr);

    void variable_declaration(const AstNode<VarStmt>& expr);
    void function_declaration(const AstNode<FunctionStmt>& stmt);
    void function(const AstNode<FunctionStmt>& stmt, FunctionType type);

    void constructor(
        const Constructor& stmt,
        const std::vector<Field>& fields,
        bool has_superclass,
        int superclass_arguments_count
    );

    void default_constructor(const std::vector<Field>& fields, bool has_superclass);

    void using_core(const std::vector<AstNode<UsingStmt>>& using_stmts);

    void class_core(
        int class_slot,
        std::optional<Token> super_class,
        const std::vector<Method>& methods,
        const std::vector<Field>& fields,
        const std::vector<AstNode<UsingStmt>>& using_stmts,
        bool is_abstract
    );

    void class_declaration(const AstNode<ClassStmt>& stmt);
    void expr_statement(const AstNode<ExprStmt>& stmt);
    void native_declaration(const AstNode<NativeStmt>& stmt);

    void if_expression(const AstNode<IfExpr>& stmt);

    void visit_expr(const Expr& expr);

    void block(const AstNode<BlockExpr>& expr);
    void loop_expression(const AstNode<LoopExpr>& expr);
    void pop_out_of_scopes(int64_t depth);
    void break_expr(const AstNode<BreakExpr>& expr);
    void continue_expr(const AstNode<ContinueExpr>& expr);
    void while_expr(const AstNode<WhileExpr>& expr);
    void for_expr(const AstNode<ForExpr>& expr);
    void return_expression(const AstNode<ReturnExpr>& stmt);

    void literal(const AstNode<LiteralExpr>& expr);
    void string_literal(const AstNode<StringLiteral>& expr);
    void unary(const AstNode<UnaryExpr>& expr);

    void binary(const AstNode<BinaryExpr>& expr);
    void emit_set_variable(const bite::Analyzer::Binding& binding);

    void set_variable(const Expr& lvalue);
    void emit_get_variable(const bite::Analyzer::Binding& binding);

    void logical(const AstNode<BinaryExpr>& expr);

    void variable(const AstNode<VariableExpr>& expr);
    void call(const AstNode<CallExpr>& expr);
    void get_property(const AstNode<GetPropertyExpr>& expr);
    void super(const AstNode<SuperExpr>& expr);

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
