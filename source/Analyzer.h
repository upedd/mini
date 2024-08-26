#ifndef ANALYZER_H
#define ANALYZER_H
#include <string>
#include <algorithm>
#include <utility>

#include "Ast.h"
#include "AstVisitor.h"
#include "base/logger.h"
#include "shared/SharedContext.h"

namespace bite {
    /**
     * Performs semantic analysis
     * It is the last step in compiler fronted which will check if produced ast is semantically valid
     * It will also resolve variable bindings
     */
    class Analyzer : MutatingAstVisitor {
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
        void block_expr(BlockExpr& expr);
        void variable_declaration(VariableDeclaration& stmt);
        void variable_expr(VariableExpr& expr);
        void expr_stmt(ExprStmt& stmt);
        void function_declaration(FunctionDeclaration& box);
        void function(FunctionDeclaration& stmt);
        void native_declaration(NativeDeclaration& stmt);
        void class_declaration(ClassDeclaration& box);
        void class_object(ClassObject& object, bool is_abstract, SourceSpan& name_span);
        void object_declaration(ObjectDeclaration& stmt);
        void unary_expr(UnaryExpr& expr);
        void binary_expr(BinaryExpr& expr);
        void call_expr(CallExpr& expr);
        void get_property_expr(GetPropertyExpr& expr);
        void if_expr(IfExpr& expr);
        void loop_expr(LoopExpr& expr);
        void break_expr(BreakExpr& expr);
        void continue_expr(ContinueExpr& expr);
        void while_expr(WhileExpr& expr);
        void for_expr(ForExpr& expr);
        void return_expr(ReturnExpr& expr);
        void this_expr(ThisExpr& expr);
        void string_expr(StringExpr& /*unused*/) {}
        void super_expr(SuperExpr& expr);
        void object_expr(ObjectExpr& expr);
        void trait_declaration(TraitDeclaration& stmt);
        void invalid_stmt(InvalidStmt& /*unused*/) {}
        void invalid_expr(InvalidExpr& /*unused*/) {}
        void literal_expr(LiteralExpr& /*unused*/) {}

        void with_context(AstNode& node, const auto& fn) {
            context_nodes.push_back(&node);
            fn();
            context_nodes.pop_back();
        }

        void declare_in_function_enviroment(
            FunctionEnviroment& env,
            StringTable::Handle name,
            AstNode* declaration,
            DeclarationInfo* declaration_info
        );

        void declare_in_class_enviroment(ClassEnviroment& env, StringTable::Handle name, const MemberInfo& attributes);


        void declare_in_trait_enviroment(TraitEnviroment& env, StringTable::Handle name, const MemberInfo& attributes);

        void declare_in_global_enviroment(
            GlobalEnviroment& env,
            StringTable::Handle name,
            AstNode* declaration,
            DeclarationInfo* declaration_info
        );

        // declare variable
        void declare(StringTable::Handle name, AstNode* declaration, DeclarationInfo* declaration_info);

        ClassEnviroment* current_class_enviroment();

        TraitEnviroment* current_trait_enviroment();

        // TODO: refactor!
        void declare_in_outer(StringTable::Handle name, AstNode* declaration, DeclarationInfo* declaration_info);


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

        static bool node_is_function(const AstNode* node) {
            return node->is_function_declaration();
        }

        bool is_in_function();

        bool is_there_matching_label(StringTable::Handle label_name);

        bool is_in_class();

        bool is_in_class_with_superclass();

        void with_scope(const auto& fn) {
            for (auto *node : context_nodes | std::views::reverse) {
                if (node->is_function_declaration()) {
                    auto* function = node->as_function_declaration();
                    function->enviroment.locals.scopes.emplace_back();
                    fn();
                    function->enviroment.locals.scopes.pop_back();
                    return;
                }
            }
            ast->enviroment.locals.scopes.emplace_back();
            fn();
            ast->enviroment.locals.scopes.pop_back();
        }

        std::optional<AstNode*> find_declaration(StringTable::Handle name, const SourceSpan& span);

        void trait_usage(
            const auto& fn,
            unordered_dense::map<StringTable::Handle, MemberInfo>& requirements,
            TraitUsage& trait_usage
        ) {
            TraitDeclaration* item_trait = nullptr;
            trait_usage.binding = resolve(trait_usage.trait.string, trait_usage.trait.span);
            if (auto declaration = find_declaration(trait_usage.trait.string, trait_usage.span)) {
                if (declaration.value()->is_trait_declaration()) {
                    item_trait = declaration.value()->as_trait_declaration();
                } else {
                    emit_error_diagnostic(
                        "using item must be a trait",
                        trait_usage.span,
                        "does not point to trait type",
                        {
                            InlineHint {
                                .location = declaration.value()->span,
                                .message = "defined here",
                                .level = DiagnosticLevel::INFO
                            }
                        }
                    );
                }
            } else {
                emit_error_diagnostic(
                    "using item must be an local or global variable",
                    trait_usage.span,
                    "is not an local or global variable"
                );
            }

            if (!item_trait) {
                return;
            }

            for (auto& [field_name, field_attr] : item_trait->enviroment.members) {
                //check if excluded
                bool is_excluded = std::ranges::contains(trait_usage.exclusions, field_name, &Token::string);

                if (is_excluded || field_attr.attributes[ClassAttributes::ABSTRACT]) {
                    requirements[field_name] = field_attr;
                    // warn about useless exclude?
                    continue;
                }
                // should aliasing methods that are requierments be allowed?
                StringTable::Handle aliased_name = field_name;
                for (auto& [before, after] : trait_usage.aliases) {
                    if (before.string == field_name) {
                        aliased_name = after.string;
                        break;
                    }
                }
                fn(aliased_name, field_attr);
                trait_usage.declarations.emplace_back(field_name, aliased_name, field_attr.attributes);
            }
        }



        // TODO: refactor
        void check_member_declaration(
            SourceSpan& name_span,
            bool is_abstract,
            StringTable::Handle name,
            MemberInfo& info,
            unordered_dense::map<StringTable::Handle, MemberInfo>& overrideable_members
        );

        // // TODO: refactor?
        // void handle_constructor(
        //     Constructor& constructor,
        //     std::vector<Field>& fields,
        //     bool is_abstract,
        //     SourceSpan& name_span,
        //     ClassEnviroment* env,
        //     unordered_dense::map<StringTable::Handle, MemberInfo>& overrideable_members,
        //     ClassDeclaration* superclass
        // );
        //
        // // TODO: refactor. put class node in the object
        // void structure_body(
        //     StructureBody& body,
        //     std::optional<Token> super_class,
        //     Binding& superclass_binding,
        //     const SourceSpan& super_class_span,
        //     SourceSpan& name_span,
        //     ClassEnviroment* env,
        //     bool is_abstract
        // );

    private:
        std::vector<AstNode*> context_nodes;
        Ast* ast;

        bool m_has_errors = false;
        SharedContext* context;
    };
} // namespace bite

#endif //ANALYZER_H
