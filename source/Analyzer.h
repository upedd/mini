#ifndef ANALYZER_H
#define ANALYZER_H
#include <string>
#include <algorithm>
#include <utility>

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

        // TODOs: class analysis, tratis analysis
        // refactor: overlap with parser
        void emit_message(Logger::Level level, const std::string& content, const std::string& inline_content);

        [[nodiscard]] bool has_errors() const {
            return m_has_errors;
        }

        [[nodiscard]] const std::vector<Message>& get_messages() const {
            return messages;
        }


        void analyze(Ast& ast);
        void block(AstNode<BlockExpr>& expr);
        void variable_declarataion(AstNode<VarStmt>& stmt);
        void variable_expression(AstNode<VariableExpr>& expr);

        void expression_statement(AstNode<ExprStmt>& stmt);
        void function_declaration(AstNode<FunctionStmt>& box);
        void function(AstNode<FunctionStmt>& stmt);
        void native_declaration(AstNode<NativeStmt>& stmt);
        void class_declaration(AstNode<ClassStmt>& box);
        void object_declaration(AstNode<ObjectStmt>& stmt);
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

        void super_expr(const AstNode<SuperExpr>& expr);
        void object_expr(AstNode<ObjectExpr>& expr);
        void trait_declaration(AstNode<TraitStmt>& stmt);


        // TODO: mess?
        DeclarationInfo* set_declaration_info(Node node, DeclarationInfo info) {
            if (std::holds_alternative<StmtPtr>(node)) {
                auto statement = std::get<StmtPtr>(node);
                return std::visit(
                overloaded {
                    [info](AstNode<VarStmt>* stmt) -> DeclarationInfo* { return &((*stmt)->info = info); },
                    [](AstNode<ExprStmt>*) -> DeclarationInfo* { return nullptr; },
                    [info](AstNode<FunctionStmt>* stmt) -> DeclarationInfo* { return &((*stmt)->info = info); },
                    [info](AstNode<ClassStmt>* stmt) -> DeclarationInfo* { return &((*stmt)->info = info); },
                    [info](AstNode<NativeStmt>* stmt) -> DeclarationInfo* { return &((*stmt)->info = info); },
                    [info](AstNode<ObjectStmt>* stmt) -> DeclarationInfo* { return &((*stmt)->info = info); },
                    [info](AstNode<TraitStmt>* stmt) -> DeclarationInfo* { return &((*stmt)->info = info); },
                    [](AstNode<UsingStmt>*) -> DeclarationInfo* { return nullptr; },
                    [](AstNode<InvalidStmt>*) -> DeclarationInfo* { return nullptr; }
                },
                statement
            );
            }
            auto expr = std::get<ExprPtr>(node);
            if (std::holds_alternative<AstNode<ForExpr>*>(expr)) {
                return &((*std::get<AstNode<ForExpr>*>(expr))->info = info);
            }
            return nullptr;
        }

        // Or just store everything in ast should be smarter
        void declare_in_function_enviroment(FunctionEnviroment& env, StringTable::Handle name, Node declaration) {
            if (env.locals.scopes.empty()) {
                if (std::ranges::contains(env.parameters, name)) {
                    // TODO: better error message, point to original
                    context->diagnostics.add({
                        .level = DiagnosticLevel::ERROR,
                        .message = "duplicate parameter name",
                        .inline_hints = {
                            InlineHint {
                                .location = get_span(declaration),
                                .message = "here",
                                .level = DiagnosticLevel::ERROR
                            }
                        }
                    });
                    m_has_errors = true;
                }
                env.parameters.push_back(name);
            } else {
                for (auto& local : env.locals.scopes.back()) {
                    if (local.name == name) {
                        context->diagnostics.add({
                        .level = DiagnosticLevel::ERROR,
                        .message = "variable redeclared in the same scope",
                        .inline_hints = {
                            InlineHint {
                                .location = get_span(declaration),
                                .message = "new declaration here",
                                .level = DiagnosticLevel::ERROR
                            },
                            InlineHint {
                                .location = get_span(local.declaration->declaration),
                                .message = "originally declared here",
                                .level = DiagnosticLevel::INFO
                            }
                        }
                    });
                        m_has_errors = true;
                    }
                }
                // assert not-null probably
                DeclarationInfo* info = set_declaration_info(
                    declaration,
                    LocalDeclarationInfo {
                        .declaration = declaration,
                        .idx = env.locals.locals_count++,
                        .is_captured = false
                    }
                );
                env.locals.scopes.back().push_back(
                    Local { .declaration = reinterpret_cast<LocalDeclarationInfo*>(info), .name = name }
                );
            }
        }

        void declare_in_class_enviroment(
            ClassEnviroment& env,
            StringTable::Handle name,
            const MemberInfo& attributes
        ) {
            if (env.members.contains(name)) {
                auto& member_attr = env.members[name];
                if (member_attr.attributes[ClassAttributes::SETTER] && !member_attr.attributes[ClassAttributes::GETTER] && !attributes.attributes[
                    ClassAttributes::SETTER] && attributes.attributes[ClassAttributes::GETTER]) {
                    member_attr.attributes += ClassAttributes::GETTER;
                    return;
                }
                if (member_attr.attributes[ClassAttributes::GETTER] && !member_attr.attributes[ClassAttributes::SETTER] && !attributes.attributes[
                    ClassAttributes::GETTER] && attributes.attributes[ClassAttributes::SETTER]) {
                    member_attr.attributes += ClassAttributes::SETTER;
                    return;
                }

                context->diagnostics.add({
                    .level = DiagnosticLevel::ERROR,
                    .message = "class member name conflict",
                    .inline_hints = {
                        InlineHint {
                            .location = attributes.decl_span,
                            .message = "new declaration here",
                            .level = DiagnosticLevel::ERROR
                        },
                        InlineHint {
                            .location = env.members[name].decl_span,
                            .message = "originally declared here",
                            .level = DiagnosticLevel::INFO
                        }
                    }
                });
                m_has_errors = true;
            }
            env.members[name] = attributes;
        }


        void declare_in_trait_enviroment(
            TraitEnviroment& env,
            StringTable::Handle name,
            const MemberInfo& attributes
        ) {
            if (env.members.contains(name)) {
                auto& member_attr = env.members[name];
                if (member_attr.attributes[ClassAttributes::SETTER] && !member_attr.attributes[ClassAttributes::GETTER] && !attributes.attributes[
                    ClassAttributes::SETTER] && attributes.attributes[ClassAttributes::GETTER]) {
                    member_attr.attributes += ClassAttributes::GETTER;
                    return;
                    }
                if (member_attr.attributes[ClassAttributes::GETTER] && !member_attr.attributes[ClassAttributes::SETTER] && !attributes.attributes[
                    ClassAttributes::GETTER] && attributes.attributes[ClassAttributes::SETTER]) {
                    member_attr.attributes += ClassAttributes::SETTER;
                    return;
                    }
                context->diagnostics.add({
                    .level = DiagnosticLevel::ERROR,
                    .message = "trait member name conflict",
                    .inline_hints = {
                        InlineHint {
                            .location = attributes.decl_span,
                            .message = "new declaration here",
                            .level = DiagnosticLevel::ERROR
                        },
                        InlineHint {
                            .location = env.members[name].decl_span,
                            .message = "originally declared here",
                            .level = DiagnosticLevel::INFO
                        }
                    }
                });
                m_has_errors = true;
            }
            env.members[name] = attributes;
        }

        void declare_in_global_enviroment(GlobalEnviroment& env, StringTable::Handle name, Node declaration) {
            if (env.locals.scopes.empty()) {
                if (env.globals.contains(name)) {
                        context->diagnostics.add({
                        .level = DiagnosticLevel::ERROR,
                        .message = "global variable name conflict",
                        .inline_hints = {
                            InlineHint {
                                .location = get_span(declaration),
                                .message = "new declaration here",
                                .level = DiagnosticLevel::ERROR
                            },
                            InlineHint {
                                .location = get_span(env.globals[name]->declaration),
                                .message = "originally declared here",
                                .level = DiagnosticLevel::INFO
                            }
                        }
                    });
                    m_has_errors = true;
                }
                DeclarationInfo* info = set_declaration_info(
                    declaration,
                    GlobalDeclarationInfo { .declaration = declaration, .name = name }
                );
                env.globals[name] = reinterpret_cast<GlobalDeclarationInfo*>(info);
            } else {
                for (auto& local : env.locals.scopes.back()) {
                    if (local.name == name) {
                        context->diagnostics.add({
                        .level = DiagnosticLevel::ERROR,
                        .message = "variable redeclared in the same scope",
                        .inline_hints = {
                            InlineHint {
                                .location = get_span(declaration),
                                .message = "new declaration here",
                                .level = DiagnosticLevel::ERROR
                            },
                            InlineHint {
                                .location = get_span(local.declaration->declaration),
                                .message = "originally declared here",
                                .level = DiagnosticLevel::INFO
                            }
                        }
                    });
                        m_has_errors = true;
                    }
                }
                DeclarationInfo* info = set_declaration_info(
                    declaration,
                    LocalDeclarationInfo {
                        .declaration = declaration,
                        .idx = env.locals.locals_count++,
                        .is_captured = false
                    }
                );
                env.locals.scopes.back().push_back(
                    Local { .declaration = reinterpret_cast<LocalDeclarationInfo*>(info), .name = name }
                );
            }
        }

        // declare variable
        void declare(StringTable::Handle name, Node declaration) {
            for (auto node : node_stack | std::views::reverse) {
                if (std::holds_alternative<StmtPtr>(node)) {
                    auto stmt = std::get<StmtPtr>(node);
                    if (std::holds_alternative<AstNode<FunctionStmt>*>(stmt)) {
                        auto* function = std::get<AstNode<FunctionStmt>*>(stmt);
                        declare_in_function_enviroment((*function)->enviroment, name, declaration);
                        return;
                    }
                }
            }
            declare_in_global_enviroment(ast->enviroment, name, declaration);
        }

        ClassEnviroment* current_class_enviroment() {
            for (auto node : node_stack | std::views::reverse) {
                if (std::holds_alternative<StmtPtr>(node)) {
                    auto stmt = std::get<StmtPtr>(node);
                    if (std::holds_alternative<AstNode<ClassStmt>*>(stmt)) {
                        auto* klass = std::get<AstNode<ClassStmt>*>(stmt);
                        return &(*klass)->enviroment;
                    }
                }
                if (std::holds_alternative<ExprPtr>(node)) {
                    auto expr = std::get<ExprPtr>(node);
                    if (std::holds_alternative<AstNode<ObjectExpr>*>(expr)) {
                        auto* object = std::get<AstNode<ObjectExpr>*>(expr);
                        return &(*object)->class_enviroment;
                    }
                }
            }
            return nullptr;
        }

        TraitEnviroment* current_trait_enviroment() {
            for (auto node : node_stack | std::views::reverse) {
                if (std::holds_alternative<StmtPtr>(node)) {
                    auto stmt = std::get<StmtPtr>(node);
                    if (std::holds_alternative<AstNode<TraitStmt>*>(stmt)) {
                        auto* klass = std::get<AstNode<TraitStmt>*>(stmt);
                        return &(*klass)->enviroment;
                    }
                }
            }
            return nullptr;
        }

        // TODO: refactor!
        void declare_in_outer(StringTable::Handle name, StmtPtr declaration) {
            bool skipped = false;
            for (auto node : node_stack | std::views::reverse) {
                if (std::holds_alternative<StmtPtr>(node)) {
                    auto stmt = std::get<StmtPtr>(node);
                    if (std::holds_alternative<AstNode<FunctionStmt>*>(stmt)) {
                        if (!skipped) {
                            skipped = true;
                            continue;
                        }
                        auto* function = std::get<AstNode<FunctionStmt>*>(stmt);
                        declare_in_function_enviroment((*function)->enviroment, name, declaration);
                        return;
                    }
                }
            }
            declare_in_global_enviroment(ast->enviroment, name, declaration);
        }


        static std::optional<Binding> get_binding_in_function_enviroment(
            const FunctionEnviroment& env,
            StringTable::Handle name
        ) {
            for (const auto& [idx, param] : env.parameters | std::views::enumerate) {
                if (param == name) {
                    return ParameterBinding { idx };
                }
            }
            for (const auto& scope : env.locals.scopes | std::views::reverse) {
                for (const auto& local : scope) {
                    if (local.name == name) {
                        return LocalBinding { .info = local.declaration };
                    }
                }
            }
            return {};
        }

        std::optional<Binding> get_binding_in_class_enviroment(const ClassEnviroment& env, StringTable::Handle name, const SourceSpan& source) {
            for (const auto& member : env.members | std::views::keys) {
                if (member == name) {
                    return MemberBinding { member };
                }
            }
            if (env.class_object_enviroment) {
                for (const auto& member : env.class_object_enviroment->members | std::views::keys) {
                    if (member == name) {
                        // TODO: disallow defining anything with class name inside of class otherwise this will not work
                        return ClassObjectBinding { .class_binding = resolve(env.class_name, source), .name = member };
                    }
                }
            }
            return {};
        }

        std::optional<Binding> get_binding_in_trait_enviroment(const TraitEnviroment& env, StringTable::Handle name) {
            for (const auto& member : env.members | std::views::keys) {
                if (member == name) {
                    return MemberBinding { member };
                }
            }
            return {};
        }


        static std::optional<Binding> get_binding_in_global_enviroment(
            const GlobalEnviroment& env,
            StringTable::Handle name
        ) {
            for (const auto& [global_name, declaration] : env.globals) {
                if (global_name == name) {
                    return GlobalBinding { declaration };
                }
            }
            for (const auto& scope : env.locals.scopes | std::views::reverse) {
                for (const auto& local : scope) {
                    if (local.name == name) {
                        return LocalBinding { .info = local.declaration };
                    }
                }
            }
            return {};
        }

        void capture_local(LocalDeclarationInfo* info) {
            info->is_captured = true;
        }

        int64_t add_upvalue(FunctionEnviroment& enviroment, const UpValue& upvalue) {
            auto found = std::ranges::find(enviroment.upvalues, upvalue);
            if (found != enviroment.upvalues.end()) {
                return std::distance(enviroment.upvalues.begin(), found);
            }
            enviroment.upvalues.push_back(upvalue);
            return enviroment.upvalues.size() - 1;
        }

        int64_t handle_closure(
            const std::vector<std::reference_wrapper<FunctionEnviroment>>& enviroments_visited,
            StringTable::Handle name,
            int64_t local_index
        ) {
            bool is_local = true;
            int64_t value = local_index;
            for (const auto& enviroment : enviroments_visited | std::views::reverse) {
                value = add_upvalue(enviroment, UpValue { .idx = value, .local = is_local });
                if (is_local) {
                    // Only top level can be pointing to an local variable
                    is_local = false;
                }
            }
            return value;
        }

        // TODO: refactor?
        Binding resolve_without_upvalues(StringTable::Handle name, const SourceSpan& span) {
            for (auto node : node_stack | std::views::reverse) {
                if (std::holds_alternative<StmtPtr>(node)) {
                    auto stmt = std::get<StmtPtr>(node);
                    if (std::holds_alternative<AstNode<FunctionStmt>*>(stmt)) {
                        auto* function = std::get<AstNode<FunctionStmt>*>(stmt);
                        if (auto binding = get_binding_in_function_enviroment((*function)->enviroment, name)) {
                            return std::move(binding.value());
                        }
                    }
                    if (std::holds_alternative<AstNode<ClassStmt>*>(stmt)) {
                        auto& klass = std::get<AstNode<ClassStmt>*>(stmt);
                        if (auto binding = get_binding_in_class_enviroment((*klass)->enviroment, name, span)) {
                            return std::move(binding.value());
                        }
                    }
                }
            }
            if (auto binding = get_binding_in_global_enviroment(ast->enviroment, name)) {
                return std::move(binding.value());
            }

            // TODO: better error
            context->diagnostics.add({
                .level = DiagnosticLevel::ERROR,
                .message = std::format("unresolved variable: {}", *name),
                .inline_hints = {
                    InlineHint {
                        .location = span,
                        .message = "here",
                        .level = DiagnosticLevel::ERROR
                    }
                }
            });
            m_has_errors = true;
            return NoBinding();
        }

        // TODO: refactor!
        Binding resolve(StringTable::Handle name, const SourceSpan& span) {
            std::vector<std::reference_wrapper<FunctionEnviroment>> function_enviroments_visited;
            for (auto node : node_stack | std::views::reverse) {
                if (std::holds_alternative<StmtPtr>(node)) {
                    auto stmt = std::get<StmtPtr>(node);
                    if (std::holds_alternative<AstNode<FunctionStmt>*>(stmt)) {
                        auto* function = std::get<AstNode<FunctionStmt>*>(stmt);
                        if (auto binding = get_binding_in_function_enviroment((*function)->enviroment, name)) {
                            if (std::holds_alternative<LocalBinding>(*binding) && !function_enviroments_visited.
                                empty()) {
                                capture_local(std::get<LocalBinding>(*binding).info);
                                return UpvalueBinding {
                                        handle_closure(
                                            function_enviroments_visited,
                                            name,
                                            std::get<LocalBinding>(*binding).info->idx
                                        )
                                    };
                            }

                            return std::move(binding.value());
                        }
                        function_enviroments_visited.emplace_back((*function)->enviroment);
                    }
                    if (std::holds_alternative<AstNode<ClassStmt>*>(stmt)) {
                        auto& klass = std::get<AstNode<ClassStmt>*>(stmt);
                        if (auto binding = get_binding_in_class_enviroment((*klass)->enviroment, name, span)) {
                            return std::move(binding.value());
                        }
                    }
                    if (std::holds_alternative<AstNode<TraitStmt>*>(stmt)) {
                        auto& trait = std::get<AstNode<TraitStmt>*>(stmt);
                        if (auto binding = get_binding_in_trait_enviroment((*trait)->enviroment, name)) {
                            return std::move(binding.value());
                        }
                    }
                } else if (std::holds_alternative<ExprPtr>(node)) {
                    auto expr = std::get<ExprPtr>(node);
                    if (std::holds_alternative<AstNode<ObjectExpr>*>(expr)) {
                        auto& object = std::get<AstNode<ObjectExpr>*>(expr);
                        if (auto binding = get_binding_in_class_enviroment((*object)->class_enviroment, name, span)) {
                            return std::move(binding.value());
                        }
                    }
                }
            }
            if (auto binding = get_binding_in_global_enviroment(ast->enviroment, name)) {
                if (std::holds_alternative<LocalBinding>(*binding) && !function_enviroments_visited.empty()) {
                    capture_local(std::get<LocalBinding>(*binding).info);
                    return UpvalueBinding {
                            handle_closure(
                                function_enviroments_visited,
                                name,
                                std::get<LocalBinding>(*binding).info->idx
                            )
                        };
                }

                return std::move(binding.value());
            }
            // TODO: better error
            context->diagnostics.add({
                .level = DiagnosticLevel::ERROR,
                .message = std::format("unresolved variable: {}", *name),
                .inline_hints = {
                    InlineHint {
                        .location = span,
                        .message = "here",
                        .level = DiagnosticLevel::ERROR
                    }
                }
            });
            m_has_errors = true;
            return NoBinding();
        }

        bool is_in_loop() {
            for (auto node : node_stack) {
                if (std::holds_alternative<ExprPtr>(node)) {
                    auto expr = std::get<ExprPtr>(node);
                    if (std::holds_alternative<AstNode<LoopExpr>*>(expr) || std::holds_alternative<AstNode<WhileExpr>*>(
                        expr
                    ) || std::holds_alternative<AstNode<ForExpr>*>(expr)) {
                        return true;
                    }
                }
            }
            return false;
        }

        static bool node_is_function(const Node& node) {
            return std::holds_alternative<StmtPtr>(node) && std::holds_alternative<AstNode<FunctionStmt>*>(
                std::get<StmtPtr>(node)
            );
        }

        bool is_in_function() {
            for (auto node : node_stack) {
                if (node_is_function(node)) {
                    return true;
                }
            }
            return false;
        }

        bool is_there_matching_label(StringTable::Handle label_name) {
            for (auto node : node_stack | std::views::reverse) {
                // labels do not cross functions
                if (node_is_function(node)) {
                    break;
                }
                if (std::holds_alternative<ExprPtr>(node)) {
                    auto expr = std::get<ExprPtr>(node);
                    if (std::holds_alternative<AstNode<BlockExpr>*>(expr)) {
                        auto label = (*std::get<AstNode<BlockExpr>*>(expr))->label;
                        if (label && label->string == label_name) {
                            return true;
                        }
                    } else if (std::holds_alternative<AstNode<WhileExpr>*>(expr)) {
                        auto label = (*std::get<AstNode<WhileExpr>*>(expr))->label;
                        if (label && label->string == label_name) {
                            return true;
                        }
                    } else if (std::holds_alternative<AstNode<LoopExpr>*>(expr)) {
                        auto label = (*std::get<AstNode<LoopExpr>*>(expr))->label;
                        if (label && label->string == label_name) {
                            return true;
                        }
                    } else if (std::holds_alternative<AstNode<ForExpr>*>(expr)) {
                        auto label = (*std::get<AstNode<ForExpr>*>(expr))->label;
                        if (label && label->string == label_name) {
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        bool is_in_class() {
            for (auto node : node_stack) {
                if (std::holds_alternative<StmtPtr>(node) && std::holds_alternative<AstNode<ClassStmt>*>(
                    std::get<StmtPtr>(node)
                ) || (std::holds_alternative<ExprPtr>(node) && std::holds_alternative<AstNode<ObjectExpr>*>(
                    std::get<ExprPtr>(node)
                ))) {
                    return true;
                }
            }
            return false;
        }

        bool is_in_class_with_superclass() {
            for (auto node : node_stack) {
                if ((std::holds_alternative<StmtPtr>(node) && std::holds_alternative<AstNode<ClassStmt>
                        *>(std::get<StmtPtr>(node)) && (*std::get<AstNode<ClassStmt>*>(std::get<StmtPtr>(node)))->
                    super_class) || (std::holds_alternative<ExprPtr>(node) && std::holds_alternative<AstNode<ObjectExpr>
                    *>(
                    std::get<ExprPtr>(node)
                ) && (*std::get<AstNode<ObjectExpr>*>(std::get<ExprPtr>(node)))->super_class)) {
                    return true;
                }
            }
            return false;
        }

        void with_scope(const auto& fn) {
            for (auto node : node_stack | std::views::reverse) {
                if (node_is_function(node)) {
                    auto* function = std::get<AstNode<FunctionStmt>*>(std::get<StmtPtr>(node));
                    (*function)->enviroment.locals.scopes.emplace_back();
                    fn();
                    (*function)->enviroment.locals.scopes.pop_back();
                    return;
                }
            }
            ast->enviroment.locals.scopes.emplace_back();
            fn();
            ast->enviroment.locals.scopes.pop_back();
        }

    private:
        std::vector<Node> node_stack;
        Ast* ast;
        void visit_stmt(Stmt& statement);
        void visit_expr(Expr& expression);

        bool m_has_errors = false;
        std::vector<Message> messages;
        std::string file_path;
        SharedContext* context;
    };
} // namespace bite

#endif //ANALYZER_H
