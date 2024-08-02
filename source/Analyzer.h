#ifndef ANALYZER_H
#define ANALYZER_H
#include <string>

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
        explicit Analyzer(SharedContext* context) : context(context) {}

        // TODOs: core, variable binding, upvalues?, class analysis, tratis analysis, lvalue analysis, useless break and continue, label validation
        // TODO: first variable binding
        // refactor: overlap with parser
        // TODO: traits
        // TODO: circular variables
        void emit_message(Logger::Level level, const std::string& content, const std::string& inline_content);

        [[nodiscard]] bool has_errors() const {
            return m_has_errors;
        }

        [[nodiscard]] const std::vector<Message>& get_messages() const {
            return messages;
        }


        void analyze(const Ast& ast);
        void block(const AstNode<BlockExpr>& expr);
        void variable_declarataion(const AstNode<VarStmt>& stmt);
        void variable_expression(const AstNode<VariableExpr>& expr);

        void expression_statement(const AstNode<ExprStmt>& stmt);
        void function_declaration(const AstNode<FunctionStmt>& box);
        void native_declaration(const AstNode<NativeStmt>& box);
        void class_declaration(const AstNode<ClassStmt>& box);
        void unary(const AstNode<UnaryExpr>& expr);
        void binary(const AstNode<BinaryExpr>& expr);
        void call(const AstNode<CallExpr>& expr);
        void get_property(const AstNode<GetPropertyExpr>& expr);
        void if_expression(const AstNode<IfExpr>& expr);
        void loop_expression(const AstNode<LoopExpr>& expr);
        void break_expr(const AstNode<BreakExpr>& expr);
        void while_expr(const AstNode<WhileExpr>& expr);
        void for_expr(const AstNode<ForExpr>& expr);
        void return_expr(const AstNode<ReturnExpr>& expr);

        struct LocalBinding {
            std::int64_t local_idx;
        };

        struct ParameterBinding {
            std::int64_t param_idx;
        };

        struct CapturedBinding {
            std::int64_t param_idx;
        };

        struct MemberBinding {
            StringTable::Handle name;
        };

        struct ClassObjectBinding {};

        struct GlobalBinding {
            StringTable::Handle name;
        };

        struct NoBinding {};

        using Binding = std::variant<LocalBinding, ParameterBinding, CapturedBinding, MemberBinding, ClassObjectBinding,
                                     GlobalBinding, NoBinding>;

        struct Local {
            std::int64_t idx;
            StringTable::Handle name;
            bool is_captured; // whetever is caputred in a closure
        };

        struct Scope {
            std::vector<Local> locals;
        };

        struct FunctionEnviroment {
            std::int64_t current_local_idx = 0;
            std::vector<StringTable::Handle> captured;
            std::vector<StringTable::Handle> parameters;
            std::vector<Scope> scopes;
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
        };

        using Enviroment = std::variant<FunctionEnviroment, ClassEnviroment, GlobalEnviroment>;

        static std::optional<LocalBinding> get_local_binding(
            std::vector<Scope>& scopes,
            const StringTable::Handle name
        ) {
            std::optional<LocalBinding> binding;
            // optim?
            for (const auto& scope : scopes) {
                for (const auto& local : scope.locals) {
                    if (local.name == name) {
                        binding = { .local_idx = local.idx };
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
                        for (const auto& [idx, captured] : env.captured | std::views::enumerate) {
                            if (captured == name) {
                                return CapturedBinding { idx };
                            }
                        }
                        return get_local_binding(env.scopes, name);
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

        static void declare_in_enviroment(Enviroment& enviroment, StringTable::Handle name) {
            // TODO: error handling
            std::visit(
                overloaded {
                    [name](FunctionEnviroment& env) {
                        if (env.scopes.empty()) {
                            env.parameters.push_back(name);
                        } else {
                            env.scopes.back().locals.push_back({ .idx = env.current_local_idx++, .name = name });
                        }
                    },
                    [name](ClassEnviroment& env) {
                        env.members.insert(name);
                    },
                    [name](GlobalEnviroment& env) {
                        if (env.scopes.empty()) {
                            env.globals.insert(name);
                        } else {
                            env.scopes.back().locals.push_back({ .idx = env.current_local_idx++, .name = name });
                        }
                    }
                },
                enviroment
            );
        }

        void declare(StringTable::Handle name) {
            declare_in_enviroment(enviroment_stack.back(), name);
        }

        // TODO: missing captures
        Binding get_binding(StringTable::Handle name) {
            for (auto& enviroment : enviroment_stack | std::views::reverse) {
                if (std::optional<Binding> res = get_binding_in_enviroment(enviroment, name)) {
                    return *res;
                }
            }
            emit_message(Logger::Level::error, "cannot resolve variable: " + std::string(*name), "");

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

    private:
        std::vector<Enviroment> enviroment_stack { GlobalEnviroment() };
        void visit_stmt(const Stmt& statement);
        void visit_expr(const Expr& expression);


        bool m_has_errors = false;
        std::vector<Message> messages;
        std::string file_path;
        SharedContext* context;
    };
} // namespace bite

#endif //ANALYZER_H
