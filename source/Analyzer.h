#ifndef ANALYZER_H
#define ANALYZER_H
#include <string>
#include <algorithm>

#include "Ast.h"
#include "base/logger.h"
#include "base/overloaded.h"
#include "shared/Message.h"
#include "shared/SharedContext.h"

namespace bite {
    /**
     * Performs semantic analysis
     * It is the last step in compiler fronted which will check if produced ast is semantically valid
     * It will also resolve variable bindings
     */
    class Analyzer {
    public:
        explicit Analyzer(SharedContext* context, Ast& ast) : context(context), ast(ast) {}

        // TODOs: class analysis, tratis analysis
        // refactor: overlap with parser
        void emit_message(Logger::Level level, const std::string& content, const std::string& inline_content);

        [[nodiscard]] bool has_errors() const {
            return m_has_errors;
        }

        [[nodiscard]] const std::vector<Message>& get_messages() const {
            return messages;
        }


        void analyze();
        void block(AstNode<BlockExpr>& expr);
        void variable_declarataion(AstNode<VarStmt>& stmt);
        void variable_expression(AstNode<VariableExpr>& expr);

        void expression_statement(AstNode<ExprStmt>& stmt);
        void function_declaration(AstNode<FunctionStmt>& box);
        void function(AstNode<FunctionStmt>& stmt);
        void native_declaration(AstNode<NativeStmt>& box);
        void class_declaration(AstNode<ClassStmt>& box);
        void unary(AstNode<UnaryExpr>& expr);
        void binary(AstNode<BinaryExpr>& expr);
        void call(AstNode<CallExpr>& expr);
        void get_property(AstNode<GetPropertyExpr>& expr);
        void if_expression(AstNode<IfExpr>& expr);
        void loop_expression(AstNode<LoopExpr>& expr);
        void break_expr(AstNode<BreakExpr>& expr);
        void continue_expr(AstNode<ContinueExpr>& expr);
        void while_expr(AstNode<WhileExpr>& expr);
        void for_expr(AstNode<ForExpr>& expr);
        void return_expr(AstNode<ReturnExpr>& expr);
        void this_expr(AstNode<ThisExpr>& expr);


        using Node = std::variant<Stmt*, Expr*>;

        // Or just store everything in ast should be smarter
        void declare_in_function_enviroment(FunctionEnviroment& env, StringTable::Handle name) {
            if (env.locals.scopes.empty()) {
                if (std::ranges::contains(env.parameters, name)) {
                    emit_message(Logger::Level::error, "duplicate parameter name", "here");
                }
                env.parameters.push_back(name);
            } else {
                for (auto& local : env.locals.scopes.back()) {
                    if (local.name == name) {
                        emit_message(
                            Logger::Level::error,
                            "variable redeclared in same scope",
                            "redeclared here"
                        );
                    }
                }
                env.locals.scopes.back().push_back(
                    { .idx = env.locals.locals_count++, .name = name }
                );
            }
        }

        void declare_in_class_enviroment(ClassEnviroment& env, StringTable::Handle name) {
            if (env.members.contains(name)) {
                emit_message(Logger::Level::error, "member name conflict", "here");
            }
            env.members.insert(name);
        }

        void declare_in_global_enviroment(GlobalEnviroment& env, StringTable::Handle name) {
            if (env.locals.scopes.empty()) {
                if (env.globals.contains(name)) {
                    emit_message(Logger::Level::error, "global variable redeclaration", "here");
                }
                env.globals.insert(name);
            } else {
                for (auto& local : env.locals.scopes.back()) {
                    if (local.name == name) {
                        emit_message(
                            Logger::Level::error,
                            "variable redeclared in same scope",
                            "redeclared here"
                        );
                    }
                }
                env.locals.scopes.back().push_back(
                    { .idx = env.locals.locals_count++, .name = name }
                );
            }
        }

        // declare variable
        void new_declare(StringTable::Handle name) {
            for (auto node : node_stack | std::views::reverse) {
                if (std::holds_alternative<Stmt*>(node)) {
                    auto* stmt = std::get<Stmt*>(node);
                    if (std::holds_alternative<FunctionStmt>(*stmt)) {
                        auto& function = std::get<FunctionStmt>(*stmt);
                        declare_in_function_enviroment(function.enviroment, name);
                        return;
                    }
                    if (std::holds_alternative<ClassStmt>(*stmt)) {
                        auto& klass = std::get<ClassStmt>(*stmt);
                        declare_in_class_enviroment(klass.enviroment, name);
                        return;
                    }
                }
            }
            declare_in_global_enviroment(ast.enviroment, name);
        }

        static std::optional<Binding> get_binding_in_function_enviroment(const FunctionEnviroment& env, StringTable::Handle name) {
            for (const auto& [idx, param] : env.parameters | std::views::enumerate) {
                if (param == name) {
                    return ParameterBinding { idx };
                }
            }
            for (const auto& scope : env.locals.scopes | std::views::reverse) {
                for (const auto& local : scope) {
                    if (local.name == name) {
                        return LocalBinding {.idx = local.idx};
                    }
                }
            }
            // for (const auto& upvalue : env.upvalues) {
            //     if (upvalue.name == name) {
            //         return UpvalueBinding { upvalue.value };
            //     }
            // }
            return {};
        }
        static std::optional<Binding> get_binding_in_class_enviroment(const ClassEnviroment& env, StringTable::Handle name) {
            for (const auto& member : env.members) {
                if (member == name) {
                    return MemberBinding { member };
                }
            }
            return {};
        }

        static std::optional<Binding> get_binding_in_global_enviroment(const GlobalEnviroment& env, StringTable::Handle name) {
            for (const auto& global : env.globals) {
                if (global == name) {
                    return GlobalBinding { global };
                }
            }
            for (const auto& scope : env.locals.scopes | std::views::reverse) {
                for (const auto& local : scope) {
                    if (local.name == name) {
                        return LocalBinding {.idx = local.idx};
                    }
                }
            }
            return {};
        }


        // resolve variable
        // TODO: upvalues!
        std::optional<Binding> new_resolve(StringTable::Handle name) {
            for (auto node : node_stack | std::views::reverse) {
                if (std::holds_alternative<Stmt*>(node)) {
                    auto* stmt = std::get<Stmt*>(node);
                    if (std::holds_alternative<FunctionStmt>(*stmt)) {
                        auto& function = std::get<FunctionStmt>(*stmt);
                        return get_binding_in_function_enviroment(function.enviroment, name);
                    }
                    if (std::holds_alternative<ClassStmt>(*stmt)) {
                        auto& klass = std::get<ClassStmt>(*stmt);
                        return get_binding_in_class_enviroment(klass.enviroment, name);
                    }
                }
            }
            return get_binding_in_global_enviroment(ast.enviroment, name);
        }

        bool new_is_in_loop() {
            for (auto node : node_stack) {
                if (std::holds_alternative<Expr*>(node)) {
                    auto* expr = std::get<Stmt*>(node);
                    if (std::holds_alternative<LoopExpr>(*expr) || std::holds_alternative<WhileExpr>(*expr) || std::holds_alternative<ForExpr>(*expr)) {
                        return true;
                    }
                }
            }
            return false;
        }

        static bool node_is_function(const Node& node) {
            return std::holds_alternative<Stmt*>(node) && std::holds_alternative<FunctionStmt>(*std::get<Stmt*>(node));
        }

        bool new_is_in_function() {
            for (auto node : node_stack) {
                if (node_is_function(node)) {
                    return true;
                }
            }
            return false;
        }

        bool new_is_there_matching_label(StringTable::Handle label_name) {
            for (auto node : node_stack | std::views::reverse) {
                // labels do not cross functions
                if (node_is_function(node)) {
                    break;
                }
                if (std::holds_alternative<Expr*>(node)) {
                    auto* expr = std::get<Expr*>(node);
                    if (std::holds_alternative<BlockExpr>(*expr)) {
                        auto label = std::get<BlockExpr>(*expr).label;
                        if (label && label->string == label_name) {
                            return true;
                        }
                    } else if (std::holds_alternative<WhileExpr>(*expr)) {
                        auto label = std::get<WhileExpr>(*expr).label;
                        if (label && label->string == label_name) {
                            return true;
                        }
                    } else if (std::holds_alternative<LoopExpr>(*expr)) {
                        auto label = std::get<LoopExpr>(*expr).label;
                        if (label && label->string == label_name) {
                            return true;
                        }
                    } else if (std::holds_alternative<ForExpr>(*expr)) {
                        auto label = std::get<ForExpr>(*expr).label;
                        if (label && label->string == label_name) {
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        bool new_is_in_class() {
            for (auto node : node_stack) {
                if (std::holds_alternative<Stmt*>(node) && std::holds_alternative<ClassStmt>(*std::get<Stmt*>(node))) {
                    return true;
                }
            }
            return false;
        }

        struct Local {
            std::int64_t idx;
            StringTable::Handle name;
            std::int64_t declared_by_idx;
        };

        struct Upvalue {
            std::int64_t value;
            StringTable::Handle name;
            bool is_local; // whetever the upvalue points to a local variable or an another upvalue

            bool operator==(const Upvalue& other) const {
                return this->value == other.value && this->name == other.name && this->is_local == other.is_local;
            }
        };

        struct Scope {
            std::vector<Local> locals;
        };

        // Only used for semantic analysis
        struct SemanticScope {
            std::optional<StringTable::Handle> label;
        };

        struct FunctionEnviroment {
            std::int64_t current_local_idx = 0;
            std::vector<Upvalue> upvalues;
            std::vector<StringTable::Handle> parameters;
            std::vector<Scope> scopes;
            std::vector<SemanticScope> semantic_scopes;
        };

        // TODO: missing class object resolution
        // TODO: missing getters and setters handling
        struct ClassEnviroment {
            unordered_dense::set<StringTable::Handle> members;
        };

        struct GlobalEnviroment {
            std::int64_t current_local_idx = 0;
            unordered_dense::set<StringTable::Handle> globals;
            std::vector<Scope> scopes;
            std::vector<SemanticScope> semantic_scopes;
        };

        using Enviroment = std::variant<FunctionEnviroment, ClassEnviroment, GlobalEnviroment>;

        // refactor!
        void with_semantic_scope(const SemanticScope& scope, const auto& fn) {
            // assert env stack non-empty
            std::visit(
                overloaded {
                    [&fn, this, &scope](GlobalEnviroment&) {
                        // workaround reference pointing to wrong address after context stack resized, maybe use deque for context_stack?
                        std::get<GlobalEnviroment>(enviroment_stack.back()).semantic_scopes.push_back(scope);
                        fn();
                        std::get<GlobalEnviroment>(enviroment_stack.back()).semantic_scopes.pop_back();
                    },
                    [&fn, this, &scope](FunctionEnviroment&) {
                        std::get<FunctionEnviroment>(enviroment_stack.back()).semantic_scopes.push_back(scope);
                        fn();
                        std::get<FunctionEnviroment>(enviroment_stack.back()).semantic_scopes.pop_back();
                    },
                    [](ClassEnviroment&) {
                        // panic!
                        std::unreachable();
                    }
                },
                enviroment_stack.back()
            );
        }

        static std::optional<LocalBinding> get_local_binding(
            std::vector<Scope>& scopes,
            const StringTable::Handle name
        ) {
            std::optional<LocalBinding> binding;
            // optim?
            for (const auto& scope : scopes) {
                for (const auto& local : scope.locals) {
                    if (local.name == name) {
                        binding = LocalBinding { .idx = local.idx };
                    }
                }
            }
            return binding;
        }

        static std::optional<Binding> get_binding_in_enviroment(Enviroment& enviroment, StringTable::Handle name) {
            return std::visit(
                overloaded {
                    [name](FunctionEnviroment& env) -> std::optional<Binding> {
                        for (const auto& [idx, param] : env.parameters | std::views::enumerate) {
                            if (param == name) {
                                return ParameterBinding { idx };
                            }
                        }
                        if (auto local = get_local_binding(env.scopes, name)) {
                            return local;
                        }

                        for (const auto& upvalue : env.upvalues) {
                            if (upvalue.name == name) {
                                return UpvalueBinding { upvalue.value };
                            }
                        }
                        return {};
                    },
                    [name](const ClassEnviroment& env) -> std::optional<Binding> {
                        for (const auto& member : env.members) {
                            if (member == name) {
                                return MemberBinding { member };
                            }
                        }
                        return {};
                    },
                    [name](GlobalEnviroment& env) -> std::optional<Binding> {
                        for (const auto& global : env.globals) {
                            if (global == name) {
                                return GlobalBinding { global };
                            }
                        }
                        return get_local_binding(env.scopes, name);
                    }
                },
                enviroment
            );
        }

        void declare_in_enviroment(Enviroment& enviroment, StringTable::Handle name, std::int64_t declaration_idx) {
            // TODO: error handling
            std::visit(
                overloaded {
                    [this, name, declaration_idx](FunctionEnviroment& env) {
                        if (env.scopes.empty()) {
                            if (std::ranges::contains(env.parameters, name)) {
                                emit_message(Logger::Level::error, "duplicate parameter name", "here");
                            }
                            env.parameters.push_back(name);
                        } else {
                            for (auto& local : env.scopes.back().locals) {
                                if (local.name == name) {
                                    emit_message(
                                        Logger::Level::error,
                                        "variable redeclared in same scope",
                                        "redeclared here"
                                    );
                                }
                            }
                            env.scopes.back().locals.push_back(
                                { .idx = env.current_local_idx++, .name = name, .declared_by_idx = declaration_idx }
                            );
                        }
                    },
                    [name, this](ClassEnviroment& env) {
                        if (env.members.contains(name)) {
                            emit_message(Logger::Level::error, "member name conflict", "here");
                        }
                        env.members.insert(name);
                    },
                    [name, declaration_idx, this](GlobalEnviroment& env) {
                        if (env.scopes.empty()) {
                            if (env.globals.contains(name)) {
                                emit_message(Logger::Level::error, "global variable redeclaration", "here");
                            }
                            env.globals.insert(name);
                        } else {
                            for (auto& local : env.scopes.back().locals) {
                                if (local.name == name) {
                                    emit_message(
                                        Logger::Level::error,
                                        "variable redeclared in same scope",
                                        "redeclared here"
                                    );
                                }
                            }
                            env.scopes.back().locals.push_back(
                                { .idx = env.current_local_idx++, .name = name, .declared_by_idx = declaration_idx }
                            );
                        }
                    }
                },
                enviroment
            );
        }

        void declare(StringTable::Handle name, std::int64_t declaration_idx) {
            declare_in_enviroment(enviroment_stack.back(), name, declaration_idx);
        }

        int64_t add_upvalue(FunctionEnviroment& enviroment, const Upvalue& upvalue) {
            auto found = std::ranges::find(enviroment.upvalues, upvalue);
            if (found != enviroment.upvalues.end()) {
                return std::distance(enviroment.upvalues.begin(), found);
            }
            enviroment.upvalues.push_back(upvalue);
            return enviroment.upvalues.size() - 1;
        }

        int64_t handle_closure_binding(
            const std::vector<std::reference_wrapper<FunctionEnviroment>>& enviroments_visited,
            StringTable::Handle name,
            int64_t local_index
        ) {
            bool is_local = true;
            int64_t value = local_index;
            for (const auto& enviroment : enviroments_visited | std::views::reverse) {
                value = add_upvalue(enviroment, Upvalue { .value = value, .name = name, .is_local = is_local });
                if (is_local) {
                    // Only top level can be pointing to an local variable
                    is_local = false;
                }
            }
            return value;
        }

        void capture_local(Enviroment& enviroment, int64_t local_idx) {
            std::visit(
                overloaded {
                    [local_idx, this](FunctionEnviroment& env) {
                        for (auto& scope : env.scopes) {
                            for (auto& local : scope.locals) {
                                if (local.idx == local_idx) {
                                    is_declaration_captured[local.declared_by_idx] = true;
                                    return;
                                }
                            }
                        }
                    },
                    [local_idx, this](GlobalEnviroment& env) {
                        for (auto& scope : env.scopes) {
                            for (auto& local : scope.locals) {
                                if (local.idx == local_idx) {
                                    is_declaration_captured[local.declared_by_idx] = true;
                                    return;
                                }
                            }
                        }
                    },
                    [](ClassEnviroment&) {
                        // class enviroment must never produce local binding
                        std::unreachable();
                    }
                },
                enviroment
            );
        }

        Binding get_binding(StringTable::Handle name) {
            std::vector<std::reference_wrapper<FunctionEnviroment>> function_enviroments_visited;
            for (auto& enviroment : enviroment_stack | std::views::reverse) {
                if (std::optional<Binding> res = get_binding_in_enviroment(enviroment, name)) {
                    if (std::holds_alternative<LocalBinding>(*res) && !function_enviroments_visited.empty()) {
                        capture_local(enviroment, std::get<LocalBinding>(*res).idx);
                        return UpvalueBinding {
                                handle_closure_binding(
                                    function_enviroments_visited,
                                    name,
                                    std::get<LocalBinding>(*res).idx
                                )
                            };
                    }
                    return *res;
                }
                if (std::holds_alternative<FunctionEnviroment>(enviroment)) {
                    function_enviroments_visited.emplace_back(std::get<FunctionEnviroment>(enviroment));
                }
            }
            emit_message(Logger::Level::error, "unresolved variable: " + std::string(*name), "here");

            return NoBinding();
        }

        void with_enviroment(const Enviroment& enviroment, const auto& fn) {
            enviroment_stack.push_back(enviroment);
            fn();
            enviroment_stack.pop_back();
        }

        void with_scope(const auto& fn) {
            // assert env stack non-empty
            std::visit(
                overloaded {
                    [&fn, this](GlobalEnviroment&) {
                        // workaround reference pointing to wrong address after context stack resized, maybe use deque for context_stack?
                        std::get<GlobalEnviroment>(enviroment_stack.back()).scopes.emplace_back();
                        fn();
                        std::get<GlobalEnviroment>(enviroment_stack.back()).scopes.pop_back();
                    },
                    [&fn, this](FunctionEnviroment&) {
                        std::get<FunctionEnviroment>(enviroment_stack.back()).scopes.emplace_back();
                        fn();
                        std::get<FunctionEnviroment>(enviroment_stack.back()).scopes.pop_back();
                    },
                    [](ClassEnviroment&) {
                        // panic!
                        std::unreachable();
                    }
                },
                enviroment_stack.back()
            );
        }

        unordered_dense::map<std::size_t, Binding> bindings;
        unordered_dense::map<std::size_t, std::vector<Upvalue>> function_upvalues;
        unordered_dense::map<std::size_t, bool> is_declaration_captured;
        unordered_dense::map<std::size_t, bool> class_declaratons;


        std::vector<Node> node_stack;
    private:
        Ast& ast;
        std::vector<Enviroment> enviroment_stack { GlobalEnviroment() };
        void visit_stmt(Stmt& statement);
        void visit_expr(Expr& expression);


        bool m_has_errors = false;
        std::vector<Message> messages;
        std::string file_path;
        SharedContext* context;
    };
} // namespace bite

#endif //ANALYZER_H
