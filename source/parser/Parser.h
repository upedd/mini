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
        return bite::SourceSpan {.start_offset = 0, .end_offset = 0, .file_path = nullptr};
    }
    auto with_source_span(const auto& fn) {
        span_stack.emplace_back(next.span);
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

    std::unique_ptr<Stmt> statement_or_expression();
    std::optional<std::unique_ptr<Stmt>> statement();
    std::optional<std::unique_ptr<Stmt>> control_flow_expression_statement();
    std::unique_ptr<Stmt> expr_statement();

    std::unique_ptr<NativeDeclaration> native_declaration();
    std::unique_ptr<ObjectDeclaration> object_declaration();

    std::unique_ptr<VariableDeclaration> var_declaration();
    std::unique_ptr<VariableDeclaration> var_declaration_body(const Token& name);

    std::unique_ptr<FunctionDeclaration> function_declaration();
    std::unique_ptr<FunctionDeclaration> function_declaration_body(const Token& name, bool skip_params = false);
    std::vector<Token> functions_parameters();
    std::vector<std::unique_ptr<Expr>> call_arguments();
    TraitUsage trait_usage();

    Constructor constructor();
    ClassObject class_object();
    std::unique_ptr<ClassDeclaration> class_declaration(bool is_abstract = false);
    bitflags<ClassAttributes> member_attributes();

    Constructor constructor_statement();

    SuperConstructorCall super_constructor_call();
    std::unique_ptr<FunctionDeclaration> abstract_method(const Token& name, bool skip_params);
    std::unique_ptr<VariableDeclaration> abstract_field(const Token& name);

    std::unique_ptr<TraitDeclaration> trait_declaration();
    std::unique_ptr<FunctionDeclaration> in_trait_function(const Token& name, bitflags<ClassAttributes>& attributes, bool skip_params);

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

    std::unique_ptr<Expr> expression(Precedence precedence = Precedence::NONE);

    std::optional<std::unique_ptr<Expr>> prefix();

    std::unique_ptr<Expr> integer();
    std::unique_ptr<Expr> number();
    std::unique_ptr<Expr> keyword();
    std::unique_ptr<StringExpr> string();
    std::unique_ptr<VariableExpr> identifier();
    std::unique_ptr<Expr> grouping();
    std::unique_ptr<UnaryExpr> unary(Token::Type op);
    std::unique_ptr<SuperExpr> super_();
    std::unique_ptr<IfExpr> if_expression();
    std::unique_ptr<BreakExpr> break_expression();
    std::unique_ptr<ContinueExpr> continue_expression();
    std::unique_ptr<ReturnExpr> return_expression();
    std::unique_ptr<ObjectExpr> object_expression();
    std::unique_ptr<Expr> labeled_expression();
    std::unique_ptr<LoopExpr> loop_expression(const std::optional<Token>& label = {});
    std::unique_ptr<WhileExpr> while_expression(const std::optional<Token>& label = {});
    std::unique_ptr<ForExpr> for_expression(const std::optional<Token>& label = {});
    std::unique_ptr<BlockExpr> block(const std::optional<Token>& label = {});

    std::unique_ptr<Expr> infix(std::unique_ptr<Expr> left);
    std::unique_ptr<GetPropertyExpr> dot(std::unique_ptr<Expr> left);
    std::unique_ptr<BinaryExpr> binary(std::unique_ptr<Expr> left);
    std::unique_ptr<BinaryExpr> assigment(std::unique_ptr<Expr> left);
    std::unique_ptr<CallExpr> call(std::unique_ptr<Expr> left);

    Token current;
    Token next;
    Lexer lexer;
    SharedContext* context;
    Ast ast;
};


#endif //PARSER_H
