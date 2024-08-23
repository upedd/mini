#ifndef PARSER_H
#define PARSER_H

#include <vector>

#include "Lexer.h"
#include "../Ast.h"
#include "../base/debug.h"
#include "../shared/Message.h"

/**
 * Implementation of Pratt parser
 * References:
 * https://journal.stuffwithstuff.com/2011/03/19/pratt-parsers-expression-parsing-made-easy/
 * https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html
 * https://en.wikipedia.org/w/index.php?title=Operator-precedence_parser
 * https://github.com/munificent/bantam/tree/master
 */
class Parser {
public:
    /**
     *
     * @return messages generated during parsing
     */
    [[nodiscard]] const std::vector<bite::Message>& get_messages() const {
        return messages;
    }

    /**
     *
     * @return whetever the parser emitted any error messages
     */
    [[nodiscard]] bool has_errors() const {
        return m_has_errors;
    }

    // ReSharper disable once CppPossiblyUninitializedMember
    explicit Parser(bite::file_input_stream&& stream, SharedContext* context) : lexer(std::move(stream), context),
        context(context) {}

    Ast parse();

private:
    // Parser handles errors by quietly storing them instead of stopping.
    // We want to continue parsing in order to provide user multiple issues in their code at once.
    bool panic_mode = false;
    bool m_has_errors = false;
    std::vector<bite::Message> messages;

    std::int64_t current_span_start = -1;
    std::int64_t current_span_end = -1;

    bite::SourceSpan make_span() {
        BITE_ASSERT(!span_stack.empty());
        BITE_ASSERT(span_stack.back().start_offset != -1);
        BITE_ASSERT(span_stack.back().end_offset != -1);
        return span_stack.back();
    }

    // TODO: remove unsafe
    bite::SourceSpan no_span() {
        return bite::SourceSpan {.start_offset = 0, .end_offset = 0, .file_path = "unknown"};
    }

    auto with_source_span(const auto& fn) {
        span_stack.emplace_back(next.source_start_offset, next.source_end_offset, lexer.get_filepath());
        auto res = fn();
        span_stack.pop_back();
        return res;
    }

    std::vector<bite::SourceSpan> span_stack;

    void emit_message(const bite::Message& message);
    void error(const Token& token, const std::string& message, const std::string& inline_message = "");
    void warning(const Token& token, const std::string& message, const std::string& inline_message = "");

    void synchronize();

    bool match(Token::Type type);
    [[nodiscard]] bool check(Token::Type type) const;

    Token advance();
    void consume(Token::Type type, const std::string& message);

    StringTable::Handle context_keyword(const std::string& keyword) const;

    Stmt statement_or_expression();
    std::optional<Stmt> statement();
    std::optional<Stmt> control_flow_expression_statement();
    Stmt expr_statement();

    Stmt native_declaration();
    Stmt object_declaration();

    Stmt var_declaration();
    AstNode<VarStmt> var_declaration_body(const Token& name);

    AstNode<FunctionStmt> function_declaration();
    AstNode<FunctionStmt> function_declaration_body(const Token& name, bool skip_params = false);
    std::vector<Token> functions_parameters();
    std::vector<Expr> call_arguments();

    Stmt class_declaration(bool is_abstract = false);
    StructureBody structure_body(const Token& class_token);

    AstNode<UsingStmt> using_statement();
    UsingStmtItem using_stmt_item();
    UsingStmtItem using_stmt_item_with_params(const Token& name);

    bitflags<ClassAttributes> member_attributes();

    Constructor constructor_statement();

    Constructor default_constructor(const Token& class_token);
    AstNode<FunctionStmt> abstract_method(const Token& name, bool skip_params);
    AstNode<VarStmt> abstract_field(const Token& name);

    Stmt trait_declaration();
    AstNode<FunctionStmt> in_trait_function(const Token& name, bitflags<ClassAttributes>& attributes, bool skip_params);

    /* C like precedence
     * References:
     * https://en.wikipedia.org/wiki/Operators_in_C_and_C%2B%2B#Operator_precedence
     * https://en.cppreference.com/w/cpp/language/operator_precedence
     */
    enum class Precedence : std::uint8_t {
        NONE,
        ASSIGMENT,
        LOGICAL_OR,
        LOGICAL_AND,
        BITWISE_OR,
        BITWISE_AND,
        BITWISE_XOR,
        EQUALITY,
        RELATIONAL,
        BITWISE_SHIFT,
        TERM,
        FACTOR,
        UNARY,
        CALL,
        PRIMARY // literal or variable
    };
    static Precedence get_precendece(Token::Type token);

    Expr expression(Precedence precedence = Precedence::NONE);

    std::optional<Expr> prefix();

    Expr integer();
    Expr number();
    Expr keyword();
    Expr string();
    Expr identifier();
    Expr grouping();
    Expr unary(Token::Type op);
    Expr super_();
    Expr if_expression();
    Expr break_expression();
    Expr continue_expression();
    Expr return_expression();
    AstNode<ObjectExpr> object_expression();
    Expr labeled_expression();
    Expr loop_expression(const std::optional<Token>& label = {});
    Expr while_expression(const std::optional<Token>& label = {});
    Expr for_expression(const std::optional<Token>& label = {});
    AstNode<BlockExpr> block(const std::optional<Token>& label = {});

    Expr infix(Expr left);
    Expr dot(Expr left);
    Expr binary(Expr left);
    Expr assigment(Expr left);
    Expr call(Expr left);

    Token current;
    Token next;
    Lexer lexer;
    SharedContext* context;
    Ast ast;
};


#endif //PARSER_H
