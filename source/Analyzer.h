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

        void emit_error_diagnostic(
            const std::string& message,
            const SourceSpan& inline_errror_location,
            const std::string& inline_error_message,
            std::vector<InlineHint>&& additonal_hints = {}
        );

        [[nodiscard]] bool has_errors() const {
            return m_has_errors;
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
        DeclarationInfo* set_declaration_info(Node node, DeclarationInfo info);

        void declare_in_function_enviroment(FunctionEnviroment& env, StringTable::Handle name, Node declaration);

        void declare_in_class_enviroment(ClassEnviroment& env, StringTable::Handle name, const MemberInfo& attributes);


        void declare_in_trait_enviroment(TraitEnviroment& env, StringTable::Handle name, const MemberInfo& attributes);

        void declare_in_global_enviroment(GlobalEnviroment& env, StringTable::Handle name, Node declaration);

        // declare variable
        void declare(StringTable::Handle name, Node declaration);

        ClassEnviroment* current_class_enviroment();

        TraitEnviroment* current_trait_enviroment();

        // TODO: refactor!
        void declare_in_outer(StringTable::Handle name, StmtPtr declaration);


        static std::optional<Binding> get_binding_in_function_enviroment(
            const FunctionEnviroment& env,
            StringTable::Handle name
        );

        std::optional<Binding> get_binding_in_class_enviroment(
            const ClassEnviroment& env,
            StringTable::Handle name,
            const SourceSpan& source
        );

        std::optional<Binding> get_binding_in_trait_enviroment(const TraitEnviroment& env, StringTable::Handle name);


        static std::optional<Binding> get_binding_in_global_enviroment(
            const GlobalEnviroment& env,
            StringTable::Handle name
        );

        void capture_local(LocalDeclarationInfo* info);

        int64_t add_upvalue(FunctionEnviroment& enviroment, const UpValue& upvalue);

        int64_t handle_closure(
            const std::vector<std::reference_wrapper<FunctionEnviroment>>& enviroments_visited,
            StringTable::Handle name,
            int64_t local_index
        );

        // TODO: refactor?
        Binding resolve_without_upvalues(StringTable::Handle name, const SourceSpan& span);

        // TODO: refactor!
        Binding resolve(StringTable::Handle name, const SourceSpan& span);

        bool is_in_loop();

        static bool node_is_function(const Node& node) {
            return std::holds_alternative<StmtPtr>(node) && std::holds_alternative<AstNode<FunctionStmt>*>(
                std::get<StmtPtr>(node)
            );
        }

        bool is_in_function();

        bool is_there_matching_label(StringTable::Handle label_name);

        bool is_in_class();

        bool is_in_class_with_superclass();

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

        std::optional<Node> find_declaration(StringTable::Handle name, const SourceSpan& span) {
            auto binding = resolve_without_upvalues(name, span);
            if (std::holds_alternative<GlobalBinding>(binding)) {
                return std::get<GlobalBinding>(binding).info->declaration;
            }
            if (std::holds_alternative<LocalBinding>(binding)) {
                return std::get<LocalBinding>(binding).info->declaration;
            }
            return {};
        }

        template <typename T>
        std::optional<T> stmt_from_node(Node node) {
            if (std::holds_alternative<StmtPtr>(node) && std::holds_alternative<T>(std::get<StmtPtr>(node))) {
                return std::get<T>(std::get<StmtPtr>(node));
            }
            return node;
        }

        void using_stmt(AstNode<UsingStmt>& stmt, ClassEnviroment* env, unordered_dense::map<StringTable::Handle, MemberInfo>& requirements) {
            for (auto& item : stmt->items) {
                // Overlap with class
                // TODO: better error messages, refactor?
                AstNode<TraitStmt>* item_trait = nullptr;
                item.binding = resolve(item.name.string, item.span);
                if (auto declaration = find_declaration(item.name.string, item.span)) {
                    if (auto item_trait_stmt = stmt_from_node<AstNode<TraitStmt>*>(*declaration)) {
                        item_trait = *item_trait_stmt;
                    } else {
                        emit_error_diagnostic(
                            "using item must be a trait",
                            item.span,
                            "does not point to trait type",
                            {
                                InlineHint {
                                    .location = get_span(*declaration),
                                    .message = "defined here",
                                    .level = DiagnosticLevel::INFO
                                }
                            }
                        );
                    }
                } else {
                    emit_error_diagnostic(
                        "using item must be an local or global variable",
                        item.span,
                        "is not an local or global variable"
                    );
                }

                if (!item_trait) {
                    continue;
                }

                // TODO: prob refactor?
                for (auto& [field_name, field_attr] : (*item_trait)->enviroment.members) {
                    //check if excluded
                    bool is_excluded = std::ranges::contains(item.exclusions, field_name, &Token::string);

                    if (is_excluded || field_attr.attributes[ClassAttributes::ABSTRACT]) {
                        requirements[field_name] = field_attr;
                        // warn about useless exclude?
                        continue;
                    }
                    // should aliasing methods that are requierments be allowed?
                    StringTable::Handle aliased_name = field_name;
                    for (auto& [before, after] : item.aliases) {
                        if (before.string == field_name) {
                            aliased_name = after.string;
                            break;
                        }
                    }
                    declare_in_class_enviroment(*env, aliased_name, field_attr);
                    item.declarations.emplace_back(field_name, aliased_name, field_attr.attributes);
                }
            }
        }

    private:
        std::vector<Node> node_stack;
        Ast* ast;
        void visit_stmt(Stmt& statement);
        void visit_expr(Expr& expression);

        bool m_has_errors = false;
        SharedContext* context;
    };
} // namespace bite

#endif //ANALYZER_H
