#ifndef PARSER_H
#define PARSER_H

#include <vector>

#include "Ast.h"
#include "parser/Lexer.h"
#include "shared/Message.h"

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
    // Parser handles errors by quietly storing them instead of stopping.
    // We want to continue parsing in order to provide user multiple issues in their code at once.
    [[nodiscard]] const std::vector<bite::Message>& get_messages() const {
        return messages;
    };

    [[nodiscard]] bool has_errors() const {
        return m_has_errors;
    }

    explicit Parser(bite::file_input_stream&& stream, SharedContext* context) : lexer(std::move(stream), context),
        context(context) {}

    Ast parse();
    std::optional<Stmt> control_flow_expression_statement();
    Stmt expression_statement();

private:
    bool panic_mode = false;
    bool m_has_errors = false;
    std::vector<bite::Message> messages;

    SharedContext* context;

    void error(const Token& token, const std::string& message, const std::string& inline_message = "");
    void synchronize();

    bool match(Token::Type type);
    [[nodiscard]] bool check(Token::Type type) const;
    Token advance();
    void consume(const Token::Type type, const std::string& message);


    Stmt statement_or_expression();

    Stmt native_declaration();

    Expr for_expression(const std::optional<Token>& label = {});

    Expr return_expression();

    Stmt object_declaration();


    std::optional<Stmt> statement();

    Stmt var_declaration();
    VarStmt var_declaration_body(const Token& name);

    VarStmt var_declaration_after_name(Token name);

    FunctionStmt function_declaration();
    std::vector<Token> consume_functions_parameters();
    std::vector<Token> consume_identifiers_list();

    FunctionStmt function_declaration_body(const Token& name, bool skip_params = false);
    std::vector<Expr> consume_call_arguments();

    ConstructorStmt constructor_statement();

    FunctionStmt abstract_method(const Token& name, bool skip_params);

    VarStmt abstract_field(const Token& name);

    std::vector<Expr> arguments_list();


    Stmt class_declaration(bool is_abstract = false);

    Stmt expr_statement();

    Expr if_expression();

    // C like precedence
    // References:
    // https://en.wikipedia.org/wiki/Operators_in_C_and_C%2B%2B#Operator_precedence
    // https://en.cppreference.com/w/cpp/language/operator_precedence
    enum class Precedence {
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

    Expr this_();

    Expr super_();

    Expr block(std::optional<Token> label = {});

    Expr loop_expression(const std::optional<Token>& label = {});
    Expr break_expression();
    Expr continue_expression();
    Expr while_expression(const std::optional<Token>& label = {});
    Expr labeled_expression();

    ObjectExpr object_expression();
    FunctionStmt in_trait_function(const Token& name, bool skip_params);
    StringTable::Handle context_keyword(const std::string& keyword) const;
    StructureBody structure_body();
    StructureBody structure_body() const;

    enum class StructureType {
        CLASS,
        OBJECT,
        ABSTRACT_CLASS,
        TRAIT
    };

    struct StructureMembers {
        std::vector<MethodStmt> methods;
        std::vector<FieldStmt> fields;
        std::unique_ptr<ConstructorStmt> constructor;
        std::optional<Expr> class_object;
        std::vector<UsingStmt> using_statements;
    };

    UsingStmt using_statement();

    StructureMembers structure_body(StructureType type);
    bitflags<ClassAttributes> member_attributes(StructureType outer_type);
    Stmt trait_declaration();
    std::optional<Expr> prefix();

    Expr integer();
    Expr number();
    Expr keyword() const;
    Expr string() const;
    Expr identifier();
    Expr grouping();
    Expr unary(Token::Type op);

    Expr dot(Expr left);

    Expr infix(Expr left);

    Expr binary(Expr left);
    Expr assigment(Expr left);
    Expr call(Expr left);

    Token current;
    Token next;
    Lexer lexer;
};


#endif //PARSER_H
