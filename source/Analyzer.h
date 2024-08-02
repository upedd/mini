#ifndef ANALYZER_H
#define ANALYZER_H
#include <string>

#include "Ast.h"
#include "base/logger.h"
#include "shared/Message.h"
#include "shared/SharedContext.h"

namespace bite {
    /**
     * Performs semantic analysis
     * It is the last step in compiler fronted which will check if produced ast is semantically valid
     * It will also resolve variables bindings
     */
    class Analyzer {
    public:
        explicit Analyzer(SharedContext* context) : context(context) {}

        // TODOs: core, variable binding, upvalues?, class analysis, tratis analysis, lvalue analysis, useless break and continue
        // TODO: first variable binding
        // refactor: overlap with parser
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

        struct BlockScope {
            std::vector<StringTable::Handle> locals;
        };

        struct Binding {
            int local_index = 0;
        };

        unordered_dense::map<Expr const*, Binding> bindings;
    private:
        void visit_stmt(const Stmt& statement);
        void visit_expr(const Expr& expression);
        
        std::vector<BlockScope> block_scopes;
        bool m_has_errors = false;
        std::vector<Message> messages;
        std::string file_path;
        SharedContext* context;
    };
} // namespace bite

#endif //ANALYZER_H
