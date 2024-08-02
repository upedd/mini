#include "Parser.h"

#include <experimental/scope>

#include "conversions.h"
#include "../shared/SharedContext.h"


// TODO: refactor message system. this stinks a bit
void Parser::emit_message(const bite::Message& message) {
    if (panic_mode) {
        return;
    }
    if (message.level == bite::Logger::Level::error) {
        panic_mode = true;
        m_has_errors = true;
    }

    messages.push_back(message);
}

void Parser::error(const Token& token, const std::string& message, const std::string& inline_message) {
    emit_message(
        bite::Message {
            .level = bite::Logger::Level::error,
            .content = message,
            .inline_msg = bite::InlineMessage {
                .location = bite::SourceLocation {
                    .file_path = lexer.get_filepath(),
                    .start_offset = token.source_start_offset,
                    .end_offset = token.source_end_offset
                },
                .content = inline_message
            }
        }
    );
}

void Parser::warning(const Token& token, const std::string& message, const std::string& inline_message) {
    emit_message(
        bite::Message {
            .level = bite::Logger::Level::warn,
            .content = message,
            .inline_msg = bite::InlineMessage {
                .location = bite::SourceLocation {
                    .file_path = lexer.get_filepath(),
                    .start_offset = token.source_start_offset,
                    .end_offset = token.source_end_offset
                },
                .content = inline_message
            }
        }
    );
}


bool is_control_flow_start(const Token::Type type) {
    return type == Token::Type::LABEL || type == Token::Type::LEFT_BRACE || type == Token::Type::LOOP || type ==
        Token::Type::WHILE || type == Token::Type::FOR || type == Token::Type::IF;
}

void Parser::synchronize() {
    if (!panic_mode) {
        return;
    }
    panic_mode = false;
    // synchonization points (https://www.ssw.uni-linz.ac.at/Misc/CC/slides/03.Parsing.pdf)
    // should synchronize on start of statement or declaration
    while (!check(Token::Type::END)) {
        if (current.type == Token::Type::SEMICOLON) {
            return;
        }
        if (is_control_flow_start(next.type)) {
            return;
        }
        advance();
    }
}

Token Parser::advance() {
    current = next;
    while (true) {
        if (auto token = lexer.next_token()) {
            next = *token;
            break;
        } else {
            emit_message(token.error());
        }
    }
    return current;
}

bool Parser::check(const Token::Type type) const {
    return next.type == type;
}

void Parser::consume(const Token::Type type, const std::string& message) {
    if (check(type)) {
        advance();
        return;
    }

    error(next, message, "expected " + Token::type_to_display(type) + " here");
}

bool Parser::match(Token::Type type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

StringTable::Handle Parser::context_keyword(const std::string& keyword) const {
    return context->intern(keyword);
}

bool is_expression_start(const Token::Type type) {
    switch (type) {
        case Token::Type::INTEGER:
        case Token::Type::NUMBER:
        case Token::Type::STRING:
        case Token::Type::TRUE:
        case Token::Type::FALSE:
        case Token::Type::NIL:
        case Token::Type::IDENTIFIER:
        case Token::Type::LEFT_PAREN:
        case Token::Type::LEFT_BRACE:
        case Token::Type::BANG:
        case Token::Type::MINUS:
        case Token::Type::TILDE:
        case Token::Type::THIS:
        case Token::Type::SUPER:
        case Token::Type::IF:
        case Token::Type::LOOP:
        case Token::Type::BREAK:
        case Token::Type::CONTINUE:
        case Token::Type::WHILE:
        case Token::Type::FOR:
        case Token::Type::LABEL:
        case Token::Type::RETURN: return true;
        default: return false;
    }
}

Ast Parser::parse() {
    advance(); // populate next
    std::vector<Stmt> stmts;
    while (!match(Token::Type::END)) {
        stmts.push_back(statement_or_expression());
    }
    return Ast(std::move(stmts));
}

Stmt Parser::statement_or_expression() {
    auto handle = std::experimental::scope_exit(
        [this] {
            if (panic_mode) {
                synchronize();
            }
        }
    );

    if (auto stmt = statement()) {
        return *stmt;
    }
    if (auto stmt = control_flow_expression_statement()) {
        return *stmt;
    }
    return expr_statement();
}

std::optional<Stmt> Parser::statement() {
    if (match(Token::Type::LET)) {
        return var_declaration();
    }
    if (match(Token::Type::FUN)) {
        return function_declaration();
    }
    if (match(Token::Type::CLASS)) {
        return class_declaration();
    }
    if (match(Token::Type::ABSTRACT)) {
        consume(Token::Type::CLASS, "missing 'class' keyword");
        return class_declaration(true);
    }
    if (match(Token::Type::NATIVE)) {
        return native_declaration();
    }
    if (match(Token::Type::OBJECT)) {
        return object_declaration();
    }
    if (match(Token::Type::TRAIT)) {
        return trait_declaration();
    }
    return {};
}

std::optional<Stmt> Parser::control_flow_expression_statement() {
    std::optional<Expr> expr;
    if (match(Token::Type::IF)) {
        expr = if_expression();
    } else if (match(Token::Type::LOOP)) {
        expr = loop_expression();
    } else if (match(Token::Type::WHILE)) {
        expr = while_expression();
    } else if (match(Token::Type::FOR)) {
        expr = for_expression();
    } else if (match(Token::Type::LABEL)) {
        expr = labeled_expression();
    } else if (match(Token::Type::RETURN)) {
        expr = return_expression();
    } else if (match(Token::Type::LEFT_BRACE)) {
        expr = block();
    }
    // There is an optional semicolon after control flow expressions statements
    match(Token::Type::SEMICOLON);
    if (!expr) {
        return {};
    }
    return ast.make_node<ExprStmt>(std::move(*expr));
}

Stmt Parser::expr_statement() {
    Stmt stmt = ExprStmt { expression() };
    consume(Token::Type::SEMICOLON, "missing semicolon after expression");
    return stmt;
}

Stmt Parser::native_declaration() {
    consume(Token::Type::IDENTIFIER, "invalid native statement");
    Token name = current;
    consume(Token::Type::SEMICOLON, "missing semicolon");
    return ast.make_node<NativeStmt>(name);
}

Stmt Parser::object_declaration() {
    consume(Token::Type::IDENTIFIER, "missing object name");
    Token name = current;
    return ast.make_node<ObjectStmt>(name, object_expression());
}

Stmt Parser::var_declaration() {
    consume(Token::Type::IDENTIFIER, "missing variable name");
    Token name = current;
    return var_declaration_body(name);
}

// The part after name
AstNode<VarStmt> Parser::var_declaration_body(const Token& name) {
    Expr expr = match(Token::Type::EQUAL) ? expression() : LiteralExpr { nil_t };
    consume(Token::Type::SEMICOLON, "missing semicolon");
    return ast.make_node<VarStmt>(name, std::move(expr));
}

AstNode<FunctionStmt> Parser::function_declaration() {
    consume(Token::Type::IDENTIFIER, "missing function name");
    Token name = current;
    return function_declaration_body(name);
}

// The part after name
AstNode<FunctionStmt> Parser::function_declaration_body(const Token& name, const bool skip_params) {
    std::vector<Token> parameters = skip_params ? std::vector<Token>() : functions_parameters();
    consume(Token::Type::LEFT_BRACE, "Expected '{' before function body");
    auto body = block();
    return ast.make_node<FunctionStmt>(name, std::move(parameters), std::move(body));
}

std::vector<Token> Parser::functions_parameters() {
    consume(Token::Type::LEFT_PAREN, "missing fuction parameters");
    std::vector<Token> parameters;
    if (!check(Token::Type::RIGHT_PAREN)) {
        do {
            consume(Token::Type::IDENTIFIER, "invalid parameter."); // TODO: better error message?
            parameters.push_back(current);
        } while (match(Token::Type::COMMA));
    }
    consume(Token::Type::RIGHT_PAREN, "unmatched ')'");
    return parameters;
}

std::vector<Expr> Parser::call_arguments() {
    std::vector<Expr> arguments;
    if (!check(Token::Type::RIGHT_PAREN)) {
        do {
            arguments.emplace_back(expression());
        } while (match(Token::Type::COMMA));
    }
    consume(Token::Type::RIGHT_PAREN, "unmatched ')'");
    return arguments;
}

Stmt Parser::class_declaration(const bool is_abstract) {
    consume(Token::Type::IDENTIFIER, "missing class name");
    Token class_name = current;

    std::optional<Token> superclass;
    if (match(Token::Type::COLON)) {
        consume(Token::Type::IDENTIFIER, "missing superclass name");
        superclass = current;
    }
    StructureBody body = structure_body();

    return ast.make_node<ClassStmt>(class_name, superclass, std::move(body), is_abstract);
}

StructureBody Parser::structure_body() {
    StructureBody body;
    consume(Token::Type::LEFT_BRACE, "missing body");
    while (!check(Token::Type::RIGHT_BRACE) && !check(Token::Type::END)) {
        // Using declarataions
        if (match(Token::Type::USING)) {
            body.using_statements.push_back(using_statement());
            continue;
        }

        // Object definition
        if (match(Token::Type::OBJECT)) {
            body.class_object = object_expression();
            continue;
        }

        // Member
        auto attributes = member_attributes();
        consume(Token::Type::IDENTIFIER, "invalid member");
        Token member_name = current;

        // Constructor
        if (member_name.string == context_keyword("init")) {
            body.constructor = constructor_statement();
            continue;
        }

        // Method
        bool skip_params = attributes[ClassAttributes::GETTER] && !check(Token::Type::LEFT_PAREN);
        if (check(Token::Type::LEFT_PAREN) || skip_params) {
            if (attributes[ClassAttributes::ABSTRACT]) {
                body.methods.emplace_back(
                    ast.make_node<MethodStmt>(abstract_method(member_name, skip_params), attributes)
                );
            } else {
                body.methods.emplace_back(
                    ast.make_node<MethodStmt>(function_declaration_body(member_name, skip_params), attributes)
                );
            }
            continue;
        }

        // Field
        attributes += ClassAttributes::GETTER;
        attributes += ClassAttributes::SETTER;
        body.fields.emplace_back(ast.make_node<FieldStmt>(var_declaration_body(member_name), attributes));
    }
    consume(Token::Type::RIGHT_BRACE, "unmatched }");
    return body;
}

AstNode<UsingStmt> Parser::using_statement() {
    std::vector<UsingStmtItem> items;
    do {
        items.push_back(using_stmt_item());
    } while (match(Token::Type::COMMA));
    consume(Token::Type::SEMICOLON, "missing semicolon after statement");
    return ast.make_node<UsingStmt>(std::move(items));
}

UsingStmtItem Parser::using_stmt_item() {
    consume(Token::Type::IDENTIFIER, "Identifier expected");
    Token item_name = current;
    if (match(Token::Type::LEFT_PAREN)) {
        return using_stmt_item_with_params(item_name);
    }
    return { .name = item_name };
}


UsingStmtItem Parser::using_stmt_item_with_params(const Token& name) {
    UsingStmtItem item { .name = name };
    do {
        if (match(Token::Type::EXCLUDE)) {
            consume(Token::Type::IDENTIFIER, "invalid exclusion item");
            item.exclusions.push_back(current);
        } else {
            consume(Token::Type::IDENTIFIER, "invalid trait composition argument");
            Token before = current;
            consume(Token::Type::AS, "invalid trait composition argument");
            consume(Token::Type::IDENTIFIER, "invalid alias");
            Token after = current;
            item.aliases.emplace_back(before, after);
        }
    } while (match(Token::Type::COMMA));
    consume(Token::Type::RIGHT_PAREN, "unmatched ')'");
    return item;
}

bitflags<ClassAttributes> Parser::member_attributes() {
    bitflags<ClassAttributes> attributes;

    auto process_attribute = [&](const ClassAttributes attribute) {
        advance();
        if (attributes[attribute]) {
            warning(current, "attribute already defined", "redefinied here");
        }
        attributes += attribute;
    };

    bool consume_arguments = true;
    while (consume_arguments) {
        switch (next.type) {
            case Token::Type::PRIVATE: process_attribute(ClassAttributes::PRIVATE);
                break;
            case Token::Type::OVERRDIE: process_attribute(ClassAttributes::OVERRIDE);
                break;
            case Token::Type::ABSTRACT: process_attribute(ClassAttributes::ABSTRACT);
                break;
            case Token::Type::GET: process_attribute(ClassAttributes::GETTER);
                break;
            case Token::Type::SET: process_attribute(ClassAttributes::SETTER);
                break;
            default: consume_arguments = false;
        }
    }

    return attributes;
}

AstNode<ConstructorStmt> Parser::constructor_statement() {
    std::vector<Token> parameters = functions_parameters();
    std::vector<Expr> super_arguments;
    bool has_super = false;
    // init(parameters*) : super(arguments*) [block]
    if (match(Token::Type::COLON)) {
        has_super = true;
        consume(Token::Type::SUPER, "missing superclass constructor call");
        if (match(Token::Type::LEFT_PAREN)) {
            super_arguments = call_arguments();
        }
    }

    consume(Token::Type::LEFT_BRACE, "missing constructor body");
    return ast.make_node<ConstructorStmt>(parameters, has_super, std::move(super_arguments), block());
}

AstNode<FunctionStmt> Parser::abstract_method(const Token& name, const bool skip_params) {
    std::vector<Token> parameters = skip_params ? std::vector<Token>() : functions_parameters();
    consume(Token::Type::SEMICOLON, "missing semicolon after declaration");
    return ast.make_node<FunctionStmt>(name, std::move(parameters)); // CHECK
}

VarStmt Parser::abstract_field(const Token& name) {
    consume(Token::Type::SEMICOLON, "missing semicolon after declaration");
    return VarStmt { .name = name, .value = {} };
}

Stmt Parser::trait_declaration() {
    consume(Token::Type::IDENTIFIER, "missing trait name");
    Token trait_name = current;
    consume(Token::Type::LEFT_BRACE, "missing trait body");
    std::vector<AstNode<FieldStmt>> fields;
    std::vector<AstNode<MethodStmt>> methods;
    std::vector<AstNode<UsingStmt>> using_statements;
    while (!check(Token::Type::RIGHT_BRACE) && !check(Token::Type::END)) {
        if (match(Token::Type::USING)) {
            using_statements.push_back(using_statement());
            continue;
        }

        auto attributes = member_attributes();
        consume(Token::Type::IDENTIFIER, "invalid trait member.");
        Token member_name = current;

        bool skip_params = attributes[ClassAttributes::GETTER] && !check(Token::Type::LEFT_PAREN);
        if (check(Token::Type::LEFT_PAREN) || skip_params) {
            methods.emplace_back(
                ast.make_node<MethodStmt>(in_trait_function(member_name, attributes, skip_params), attributes)
            );
            continue;
        }

        // TODO: This probably should not be here
        attributes += ClassAttributes::GETTER;
        attributes += ClassAttributes::SETTER;
        attributes += ClassAttributes::ABSTRACT;

        fields.emplace_back(ast.make_node<FieldStmt>(abstract_field(member_name), attributes));
    }

    consume(Token::Type::RIGHT_BRACE, "missing '}' after trait body");

    return ast.make_node<TraitStmt>(trait_name, std::move(methods), std::move(fields), std::move(using_statements));
}

AstNode<FunctionStmt> Parser::in_trait_function(
    const Token& name,
    bitflags<ClassAttributes>& attributes,
    bool skip_params
) {
    std::vector<Token> parameters = skip_params ? std::vector<Token>() : functions_parameters();
    std::optional<Expr> body;
    if (match(Token::Type::LEFT_BRACE)) {
        body = block();
    } else {
        // TODO: this probably should not be here but must wait for compiler refactor!
        attributes += ClassAttributes::ABSTRACT;
        consume(Token::Type::SEMICOLON, "missing semicolon after declaration");
    }
    return ast.make_node<FunctionStmt>(name, std::move(parameters), std::move(body));
}

Parser::Precedence Parser::get_precendece(const Token::Type token) {
    switch (token) {
        case Token::Type::PLUS:
        case Token::Type::MINUS: return Precedence::TERM;
        case Token::Type::STAR:
        case Token::Type::SLASH:
        case Token::Type::SLASH_SLASH:
        case Token::Type::PERCENT: return Precedence::FACTOR;
        case Token::Type::EQUAL_EQUAL:
        case Token::Type::BANG_EQUAL: return Precedence::EQUALITY;
        case Token::Type::LESS:
        case Token::Type::LESS_EQUAL:
        case Token::Type::GREATER:
        case Token::Type::GREATER_EQUAL: return Precedence::RELATIONAL;
        case Token::Type::LESS_LESS:
        case Token::Type::GREATER_GREATER: return Precedence::BITWISE_SHIFT;
        case Token::Type::AND: return Precedence::BITWISE_AND;
        case Token::Type::BAR: return Precedence::BITWISE_OR;
        case Token::Type::CARET: return Precedence::BITWISE_XOR;
        case Token::Type::EQUAL:
        case Token::Type::PLUS_EQUAL:
        case Token::Type::MINUS_EQUAL:
        case Token::Type::STAR_EQUAL:
        case Token::Type::SLASH_EQUAL:
        case Token::Type::SLASH_SLASH_EQUAL:
        case Token::Type::PERCENT_EQUAL:
        case Token::Type::LESS_LESS_EQUAL:
        case Token::Type::GREATER_GREATER_EQUAL:
        case Token::Type::AND_EQUAL:
        case Token::Type::CARET_EQUAL:
        case Token::Type::BAR_EQUAL: return Precedence::ASSIGMENT;
        case Token::Type::AND_AND: return Precedence::LOGICAL_AND;
        case Token::Type::BAR_BAR: return Precedence::LOGICAL_OR;
        case Token::Type::LEFT_PAREN:
        case Token::Type::LEFT_BRACE:
        case Token::Type::DOT: return Precedence::CALL;
        case Token::Type::IF:
        case Token::Type::LOOP:
        case Token::Type::WHILE:
        case Token::Type::FOR:
        case Token::Type::LABEL:
        case Token::Type::RETURN:
        case Token::Type::OBJECT: default: return Precedence::NONE;
    }
}

Expr Parser::expression(const Precedence precedence) {
    advance();
    auto left = prefix();
    if (!left) {
        error(current, "expression expected", "here");
        return InvalidExpr();
    }
    while (precedence < get_precendece(next.type)) {
        advance();
        left = infix(std::move(*left));
    }
    return std::move(*left);
}

std::optional<Expr> Parser::prefix() {
    switch (current.type) {
        case Token::Type::INTEGER: return integer();
        case Token::Type::NUMBER: return number();
        case Token::Type::STRING: return string();
        case Token::Type::TRUE:
        case Token::Type::FALSE:
        case Token::Type::THIS:
        case Token::Type::NIL: return keyword();
        case Token::Type::IDENTIFIER: return identifier();
        case Token::Type::LEFT_PAREN: return grouping();
        case Token::Type::LEFT_BRACE: return block();
        case Token::Type::BANG:
        case Token::Type::MINUS:
        case Token::Type::TILDE: return unary(current.type);
        case Token::Type::SUPER: return super_();
        case Token::Type::IF: return if_expression();
        case Token::Type::LOOP: return loop_expression();
        case Token::Type::BREAK: return break_expression();
        case Token::Type::CONTINUE: return continue_expression();
        case Token::Type::WHILE: return while_expression();
        case Token::Type::FOR: return for_expression();
        case Token::Type::LABEL: return labeled_expression();
        case Token::Type::RETURN: return return_expression();
        case Token::Type::OBJECT: return object_expression();
        default: return {};
    }
}

Expr Parser::integer() {
    std::string literal = *current.string;
    std::expected<bite_int, ConversionError> result = string_to_int(literal);
    if (!result) {
        error(current, result.error().what());
    }
    return ast.make_node<LiteralExpr>(*result);
}

Expr Parser::number() {
    std::string literal = *current.string;
    std::expected<bite_float, ConversionError> result = string_to_floating(literal);
    if (!result) {
        error(current, result.error().what());
    }
    return ast.make_node<LiteralExpr>(*result);
}

Expr Parser::keyword() {
    switch (current.type) {
        case Token::Type::NIL: return ast.make_node<LiteralExpr>(nil_t);
        case Token::Type::FALSE: return ast.make_node<LiteralExpr>(false);
        case Token::Type::TRUE: return ast.make_node<LiteralExpr>(true);
        case Token::Type::THIS: return ast.make_node<ThisExpr>();
        default: std::unreachable();
    }
}

Expr Parser::identifier() {
    return ast.make_node<VariableExpr>(current);
}

Expr Parser::string() {
    return ast.make_node<StringLiteral>(*current.string);
}

Expr Parser::grouping() {
    auto expr = expression();
    consume(Token::Type::RIGHT_PAREN, "unmatched ')'");
    return expr;
}

Expr Parser::unary(const Token::Type operator_type) {
    return ast.make_node<UnaryExpr>(expression(Precedence::UNARY), operator_type);
}

Expr Parser::super_() {
    consume(Token::Type::DOT, "missing '.' after 'super'");
    consume(Token::Type::IDENTIFIER, "missing superclass member identifier after 'super'");
    return ast.make_node<SuperExpr>(current);
}

Expr Parser::if_expression() {
    auto condition = expression();
    if (current.type != Token::Type::LEFT_BRACE) {
        error(current, "missing 'if' expression body");
    }
    auto then_stmt = block();
    std::optional<Expr> else_stmt = {};
    if (match(Token::Type::ELSE)) {
        if (match(Token::Type::IF)) {
            else_stmt = if_expression();
        } else {
            consume(Token::Type::LEFT_BRACE, "missing 'else' expression body.");
            else_stmt = block();
        }
    }
    return ast.make_node<IfExpr>(std::move(condition), std::move(then_stmt), std::move(else_stmt));
}

Expr Parser::continue_expression() {
    if (match(Token::Type::LABEL)) {
        return ast.make_node<ContinueExpr>(current);
    }
    return ast.make_node<ContinueExpr>();
}

Expr Parser::break_expression() {
    std::optional<Token> label;
    if (match(Token::Type::LABEL)) {
        label = current;
    }
    if (!is_expression_start(next.type)) {
        return ast.make_node<BreakExpr>(std::optional<Expr> {}, label);
    }
    return ast.make_node<BreakExpr>(expression(), label);
}

Expr Parser::return_expression() {
    std::optional<Expr> expr = {};
    if (is_expression_start(next.type)) {
        expr = expression();
    }
    return ast.make_node<ReturnExpr>(std::move(expr));
}

AstNode<ObjectExpr> Parser::object_expression() {
    std::optional<Token> superclass;
    std::vector<Expr> superclass_arguments;
    if (match(Token::Type::COLON)) {
        consume(Token::Type::IDENTIFIER, "missing superclass name");
        superclass = current;

        if (match(Token::Type::LEFT_PAREN)) {
            superclass_arguments = call_arguments();
        }
    }

    StructureBody body = structure_body();

    return ast.make_node<ObjectExpr>(std::move(body), superclass, std::move(superclass_arguments));
}

Expr Parser::labeled_expression() {
    Token label = current;
    consume(Token::Type::COLON, "missing colon after the label");
    if (match(Token::Type::LOOP)) {
        return loop_expression(label);
    }
    if (match(Token::Type::WHILE)) {
        return while_expression(label);
    }
    if (match(Token::Type::FOR)) {
        return for_expression(label);
    }
    if (match(Token::Type::LEFT_BRACE)) {
        return block(label);
    }
    error(label, "expression cannot be labeled", "here");
    return ast.make_node<InvalidExpr>();
}

Expr Parser::loop_expression(const std::optional<Token>& label) {
    consume(Token::Type::LEFT_BRACE, "missing 'loop' expression body");
    return ast.make_node<LoopExpr>(block(), label);
}

Expr Parser::while_expression(const std::optional<Token>& label) {
    auto condition = expression();
    if (current.type != Token::Type::LEFT_BRACE) {
        error(current, "missing 'while' loop body", "expected '{' here");
    }
    auto body = block();
    return ast.make_node<WhileExpr>(std::move(condition), std::move(body), label);
}

Expr Parser::for_expression(const std::optional<Token>& label) {
    consume(Token::Type::IDENTIFIER, "invalid 'for' item declaration.");
    Token name = current;
    consume(Token::Type::IN, "invalid 'for' loop range expression.");
    Expr iterable = expression();
    if (current.type != Token::Type::LEFT_BRACE) {
        error(current, "invalid 'for' loop body", "expected '{' here.");
    }
    Expr body = block();
    return ast.make_node<ForExpr>(name, std::move(iterable), std::move(body), label);
}

Expr Parser::block(const std::optional<Token>& label) {
    // In Bite every block is an expression which can return a value
    // the value that will be returned is the last expression without succeding semicolon.
    // we track that expression here in 'expr_at_end'
    std::vector<Stmt> stmts;
    std::optional<Expr> expr_at_end = {};
    while (!check(Token::Type::RIGHT_BRACE) && !check(Token::Type::END)) {
        if (auto stmt = statement()) {
            stmts.push_back(std::move(*stmt));
        } else {
            bool is_cntrl_flow = is_control_flow_start(next.type);
            auto expr = expression();
            // Note that there is a special case as control flow expressions are not required to have an succeding semicolon
            // and still are treated as statements. We detect here if this expression is in fact the last expression that
            // should return an value or that the user explicitly put the semicolon after to force it to be treated as a statement.
            bool expression_is_statement = is_cntrl_flow && (!check(Token::Type::RIGHT_BRACE) || current.type ==
                Token::Type::SEMICOLON);
            if (match(Token::Type::SEMICOLON) || expression_is_statement) {
                stmts.emplace_back(ExprStmt(std::move(expr)));
            } else {
                expr_at_end = std::move(expr);
                break;
            }
        }
    }
    consume(Token::Type::RIGHT_BRACE, "unmatched '}'");
    return ast.make_node<BlockExpr>(std::move(stmts), std::move(expr_at_end), label);
}

Expr Parser::infix(Expr left) {
    switch (current.type) {
        case Token::Type::STAR:
        case Token::Type::PLUS:
        case Token::Type::MINUS:
        case Token::Type::SLASH:
        case Token::Type::SLASH_SLASH:
        case Token::Type::EQUAL_EQUAL:
        case Token::Type::BANG_EQUAL:
        case Token::Type::LESS:
        case Token::Type::LESS_EQUAL:
        case Token::Type::GREATER:
        case Token::Type::GREATER_EQUAL:
        case Token::Type::LESS_LESS:
        case Token::Type::GREATER_GREATER:
        case Token::Type::AND:
        case Token::Type::BAR:
        case Token::Type::CARET:
        case Token::Type::AND_AND:
        case Token::Type::BAR_BAR:
        case Token::Type::PERCENT: return binary(std::move(left));
        case Token::Type::EQUAL:
        case Token::Type::PLUS_EQUAL:
        case Token::Type::MINUS_EQUAL:
        case Token::Type::STAR_EQUAL:
        case Token::Type::SLASH_EQUAL:
        case Token::Type::SLASH_SLASH_EQUAL:
        case Token::Type::PERCENT_EQUAL:
        case Token::Type::LESS_LESS_EQUAL:
        case Token::Type::GREATER_GREATER_EQUAL:
        case Token::Type::AND_EQUAL:
        case Token::Type::CARET_EQUAL:
        case Token::Type::BAR_EQUAL: return assigment(std::move(left));
        case Token::Type::LEFT_PAREN: return call(std::move(left));
        case Token::Type::DOT: return dot(std::move(left));
        default: return left;
    }
}


Expr Parser::dot(Expr left) {
    consume(Token::Type::IDENTIFIER, "missing identifier after dot"); // TODO: better error message
    return ast.make_node<GetPropertyExpr>(std::move(left), current);
}

Expr Parser::binary(Expr left) {
    Token::Type op = current.type;
    Precedence precedence = get_precendece(op);
    return ast.make_node<BinaryExpr>(std::move(left), expression(precedence), op);
}

Expr Parser::assigment(Expr left) {
    Token::Type op = current.type;
    Precedence precedence = get_precendece(op);
    // lower our precedence to handle right associativity
    // common technique in pratt parsers
    precedence = static_cast<Precedence>(static_cast<int>(precedence) - 1);
    return ast.make_node<BinaryExpr>(std::move(left), expression(precedence), op);
}

Expr Parser::call(Expr left) {
    std::vector<Expr> arguments = call_arguments();
    return ast.make_node<CallExpr>(std::move(left), std::move(arguments));
}
