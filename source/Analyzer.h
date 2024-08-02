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

        // TODOs: core, variable binding, upvalues?, class analysis, tratis analysis, lvalue analysis, useless break and continue
        // TODO: first variable binding
        // refactor: overlap with parser
        // TODO: traits
        void emit_message(Logger::Level level, const std::string& content, const std::string& inline_content);

        [[nodiscard]] bool has_errors() const {
            return m_has_errors;
        }

        [[nodiscard]] const std::vector<Message>& get_messages() const {
            return messages;
        }


        void analyze(const Ast& ast);
        void block(const box<BlockExpr>& expr);
        void variable_declarataion(const box<VarStmt>& stmt);
        void variable_expression(const box<VariableExpr>& expr);

        void bind(const Expr& expr, StringTable::Handle name);
        void expression_statement(const box<ExprStmt>& stmt);
        void function_declaration(const box<FunctionStmt>& box);
        void native_declaration(const box<NativeStmt>& box);
        void class_declaration(const box<ClassStmt>& box);

        struct LocalResolution {
            std::int64_t local_idx;
        };

        struct ParameterResolution {
            std::int64_t param_idx;
        };

        struct CapturedResolution {
            std::int64_t param_idx;
        };

        struct MemberResolution {
            StringTable::Handle name;
        };

        struct ClassObjectResolution {};

        struct GlobalResolution {
            StringTable::Handle name;
        };

        using Resolution = std::variant<LocalResolution, ParameterResolution, CapturedResolution, MemberResolution,
                                        ClassObjectResolution, GlobalResolution>;

        struct Local {
            StringTable::Handle name;
            bool is_captured; // whetever is caputred in a closure
        };

        struct BlockScope {
            std::vector<Local> locals;
        };

        struct FunctionScope {
            std::vector<StringTable::Handle> captured;
            std::vector<StringTable::Handle> parameters;
        };

        // TODO: missing class object resolution
        // TODO: missing getters and setters handling
        struct ClassScope {
            unordered_dense::set<StringTable::Handle> members;
        };

        struct GlobalScope {
            unordered_dense::set<StringTable::Handle> globals;
        };

        using Scope = std::variant<BlockScope, FunctionScope, ClassScope, GlobalScope>;

        static std::optional<Resolution> resolve_in_scope(const Scope& scope, StringTable::Handle name) {
            return std::visit(
                overloaded {
                    [name](const BlockScope& sc) -> std::optional<Resolution> {
                        for (const auto& [idx, local] : sc.locals | std::views::enumerate) {
                            if (local.name == name) {
                                return LocalResolution { idx };
                            }
                        }
                        return {};
                    },
                    [name](const FunctionScope& sc) -> std::optional<Resolution> {
                        for (const auto& [idx, param] : sc.parameters | std::views::enumerate) {
                            if (param == name) {
                                return ParameterResolution { idx };
                            }
                        }
                        for (const auto& [idx, captured] : sc.captured | std::views::enumerate) {
                            if (captured == name) {
                                return CapturedResolution { idx };
                            }
                        }
                        return {};
                    },
                    [name](const ClassScope& sc) -> std::optional<Resolution> {
                        for (const auto& member : sc.members) {
                            if (member == name) {
                                return MemberResolution { member };
                            }
                        }
                        return {};
                    },
                    [name](const GlobalScope& sc) -> std::optional<Resolution> {
                        for (const auto& global : sc.globals) {
                            if (global == name) {
                                return GlobalResolution { global };
                            }
                        }
                        return {};
                    }
                },
                scope
            );
        }

        static void define_in_scope(Scope& scope, StringTable::Handle name) {
            // TODO: error handling
            std::visit(overloaded {
                [name](BlockScope& sc) {
                    sc.locals.push_back({.name = name});
                },
                [name](FunctionScope& sc) {
                    sc.parameters.push_back(name);
                },
                [name](ClassScope& sc) {
                    sc.members.insert(name);
                },
                [name](GlobalScope& sc) {
                    sc.globals.insert(name);
                }
            }, scope);
        }

        void define(StringTable::Handle name) {
            define_in_scope(scopes.back(), name);
        }
        // TODO: missing captures
        Resolution resolve(StringTable::Handle name) {
            for (const auto& scope : scopes | std::views::reverse) {
                if (std::optional<Resolution> res = resolve_in_scope(scope, name)) {
                    return *res;
                }
            }
            // TODO: handle error!
        }

        void with_scope(const Scope& scope, auto fn) {
            scopes.push_back(scope);
            fn();
            scopes.pop_back();
        }

        std::vector<Scope> scopes{ GlobalScope() };

        struct Binding {
            int local_index = 0;
        };

        unordered_dense::map<Expr const*, Resolution> bindings;

    private:
        void visit_stmt(const Stmt& statement);
        void visit_expr(const Expr& expression);


        bool m_has_errors = false;
        std::vector<Message> messages;
        std::string file_path;
        SharedContext* context;
    };
} // namespace bite

#endif //ANALYZER_H
