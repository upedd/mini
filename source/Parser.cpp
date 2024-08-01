#include "Parser.h"

#include <cassert>
#include <iostream>

#include "conversions.h"
#include "shared/SharedContext.h"

void Parser::error(const Token& token, const std::string& message, const std::string& inline_message) {
    if (panic_mode)
        return;
    panic_mode = true;
    m_has_errors = true;

    messages.push_back(
        bite::Message {
            .level = bite::Logger::Level::error,
            .content = message,
            .inline_msg = bite::InlineMessage {
                .location = bite::SourceLocation {
                    .file_path = "debug.bite",
                    .start_offset = token.source_start_offset,
                    .end_offset = token.source_end_offset
                },
                .content = inline_message
            }
        }
    );
}

void Parser::synchronize() {
    if (!panic_mode)
        return;
    panic_mode = false;
    while (!check(Token::Type::END)) {
        if (current.type == Token::Type::SEMICOLON)
            return;
        // synchonization points (https://www.ssw.uni-linz.ac.at/Misc/CC/slides/03.Parsing.pdf)
        // should synchronize on start of statement or declaration
        // TODO: more synchronization points!
        switch (next.type) {
            case Token::Type::LET:
            case Token::Type::LEFT_BRACE:
            case Token::Type::IF:
            case Token::Type::WHILE:
            case Token::Type::FUN:
            case Token::Type::RETURN: return;
            default: advance();
        }
    }
}

bool is_expression_start(Token::Type type) {
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

Token Parser::advance() {
    current = next;
    while (true) {
        if (auto token = lexer.next_token()) {
            next = *token;
            break;
        } else {
            error(current, token.error().message);
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

    error(current, message, "expected " + Token::type_to_string(type) + " here");
}

bool Parser::match(Token::Type type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

Ast Parser::parse() {
    advance(); // populate next
    std::vector<Stmt> stmts;
    while (!match(Token::Type::END)) {
        stmts.push_back(statement_or_expression());
    }
    return Ast(std::move(stmts));
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
    }
    // There is an optional semicolon after control flow expressions statements
    match(Token::Type::SEMICOLON);
    if (!expr) {
        return {};
    }
    return ExprStmt(std::move(*expr));
}

Stmt Parser::expression_statement() {
    Expr expr = expression();
    consume(Token::Type::SEMICOLON, "Expected ';' after expression.");
    return ExprStmt(std::move(expr));
}

Stmt Parser::statement_or_expression() {
    if (auto stmt = statement()) {
        return *stmt;
    }
    if (auto stmt = control_flow_expression_statement()) {
        return *stmt;
    }
    return expression_statement();
}

Stmt Parser::native_declaration() {
    consume(Token::Type::IDENTIFIER, "invalid native statement");
    Token name = current;
    consume(Token::Type::SEMICOLON, "missing semicolon");
    return NativeStmt(name);
}

Expr Parser::for_expression(const std::optional<Token>& label) {
    consume(Token::Type::IDENTIFIER, "invalid 'for' item declaration.");
    Token name = current;
    consume(Token::Type::IN, "invalid 'for' loop range expression.");
    Expr iterable = expression();
    // refactor maybe?
    if (current.type != Token::Type::LEFT_BRACE) {
        error(current, "invalid 'for' loop body", "expected '{' here.");
    }
    Expr body = block();
    return ForExpr { .name = name, .iterable = std::move(iterable), .body = std::move(body), .label = label };
}

Expr Parser::return_expression() {
    std::optional<Expr> expr = {};
    if (!is_expression_start(next.type)) {
        expr = expression();
    }
    return ReturnExpr { std::move(expr) };
}

Stmt Parser::object_declaration() {
    consume(Token::Type::IDENTIFIER, "missing object name");
    Token name = current;
    return ObjectStmt { .name = name, .object = object_expression() };
}

std::optional<Stmt> Parser::statement() {
    auto exit = scope_exit(
        [this] {
            if (panic_mode)
                synchronize();
        }
    );

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

Stmt Parser::var_declaration() {
    consume(Token::Type::IDENTIFIER, "missing variable name");
    Token name = current;
    return var_declaration_body(name);
}

// The part after name
VarStmt Parser::var_declaration_body(const Token& name) {
    Expr expr = match(Token::Type::EQUAL) ? expression() : LiteralExpr { nil_t };
    consume(Token::Type::SEMICOLON, "missing semicolon");
    return VarStmt { .name = name, .value = std::move(expr) };
}

FunctionStmt Parser::function_declaration() {
    consume(Token::Type::IDENTIFIER, "missing function name");
    return function_declaration_body(current);
}

std::vector<Token> Parser::consume_functions_parameters() {
    consume(Token::Type::LEFT_PAREN, "missing fuction parameters");
    std::vector<Token> parameters;
    if (!check(Token::Type::RIGHT_PAREN)) {
        do {
            consume(Token::Type::IDENTIFIER, "invalid parameter."); // TODO: better error message?
            parameters.push_back(current);
        } while (match(Token::Type::COMMA));
    }
    consume(Token::Type::RIGHT_PAREN, "unmatched paren"); // TODO: better error message
    return parameters;
}

// The part after name
FunctionStmt Parser::function_declaration_body(const Token& name, const bool skip_params) {
    std::vector<Token> parameters = skip_params ? std::vector<Token>() : consume_functions_parameters();
    consume(Token::Type::LEFT_BRACE, "Expected '{' before function body");
    auto body = block();
    return FunctionStmt { .name = name, .params = std::move(parameters), .body = std::move(body) };
}

std::vector<Expr> Parser::consume_call_arguments() {
    std::vector<Expr> arguments;
    if (!check(Token::Type::RIGHT_PAREN)) {
        do {
            arguments.emplace_back(expression());
        } while (match(Token::Type::COMMA));
    }
    consume(Token::Type::RIGHT_PAREN, "unmatched paren"); // TODO: better error message
    return arguments;
}

ConstructorStmt Parser::constructor_statement() {
    std::vector<Token> parameters = consume_functions_parameters();
    std::vector<Expr> super_arguments;
    bool has_super = false;
    // init(parameters*) : super(arguments*) [block]
    if (match(Token::Type::COLON)) {
        has_super = true;
        consume(Token::Type::SUPER, "missing superclass constructor call");
        if (match(Token::Type::LEFT_PAREN)) {
            super_arguments = consume_call_arguments();
        }
    }

    consume(Token::Type::LEFT_BRACE, "missing constructor body");
    return ConstructorStmt {
            .parameters = parameters,
            .has_super = has_super,
            .super_arguments = std::move(super_arguments),
            .body = block()
        };
}

FunctionStmt Parser::abstract_method(const Token& name, const bool skip_params) {
    std::vector<Token> parameters = skip_params ? std::vector<Token>() : consume_functions_parameters();
    consume(Token::Type::SEMICOLON, "missing semicolon after declaration");
    return FunctionStmt { .name = name, .params = std::move(parameters), .body = {} };
}

VarStmt Parser::abstract_field(const Token& name) {
    consume(Token::Type::SEMICOLON, "missing semicolon after declaration");
    return VarStmt { .name = name, .value = {} };
}

bitflags<ClassAttributes> Parser::member_attributes(StructureType outer_type) {
    // TODO: better error reporting. current setup relies too much on good order of atributes
    bitflags<ClassAttributes> attributes;
    if (match(Token::Type::PRIVATE)) {
        attributes += ClassAttributes::PRIVATE;
    }
    if (match(Token::Type::OVERRDIE)) {
        if (attributes[ClassAttributes::PRIVATE])
            error(current, "Overrides cannot be private");
        attributes += ClassAttributes::OVERRIDE;
    }
    if (match(Token::Type::ABSTRACT)) {
        if (outer_type != StructureType::ABSTRACT_CLASS) {
            error(current, "Abstract methods can be only declared in abstract classes.");
        }
        if (attributes[ClassAttributes::PRIVATE])
            error(current, "Abstract members cannot be private");
        attributes += ClassAttributes::ABSTRACT;
    }
    if (match(Token::Type::GET)) {
        attributes += ClassAttributes::GETTER;
    }
    if (match(Token::Type::SET)) {
        attributes += ClassAttributes::SETTER;
    }
    return std::move(attributes);
}


UsingStmt Parser::using_statement() {
    // TODO: do this better!
    // TODO: validate errors!
    std::vector<UsingStmtItem> items;
    do {
        consume(Token::Type::IDENTIFIER, "Identifier expected");
        Token trait_name = current;
        std::vector<Token> exclusions;
        std::vector<std::pair<Token, Token>> aliases;
        if (match(Token::Type::LEFT_PAREN)) {
            do {
                if (match(Token::Type::EXCLUDE)) {
                    consume(Token::Type::IDENTIFIER, "Expected identifer after 'exclude'.");
                    exclusions.push_back(current);
                } else { // rename
                    consume(Token::Type::IDENTIFIER, "Expected identifer or 'exclude'.");
                    Token before = current;
                    consume(Token::Type::AS, "Expected 'as' after identifier.");
                    consume(Token::Type::IDENTIFIER, "Expected identifer after 'as'.");
                    Token after = current;
                    aliases.emplace_back(before, after);
                }
            } while (match(Token::Type::COMMA));
            consume(Token::Type::RIGHT_PAREN, "Expected ')' after using trait parameters.");
        }
        items.emplace_back(trait_name, std::move(exclusions), std::move(aliases));
    } while (match(Token::Type::COMMA));
    consume(Token::Type::SEMICOLON, "Expected ';' after using statement.");
    return UsingStmt(std::move(items));
}

StringTable::Handle Parser::context_keyword(const std::string& keyword) const {
    return context->intern(keyword);
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
        auto attributes = member_attributes(StructureType::CLASS);
        consume(Token::Type::IDENTIFIER, "invalid member");
        Token member_name = current;

        // Constructor
        if (member_name.string == context_keyword("init")) {
            body.constructor = constructor_statement();
            continue;
        }

        // Field
        if (check(Token::Type::SEMICOLON) || check(Token::Type::EQUAL)) {
            body.fields.emplace_back(var_declaration_body(member_name), attributes);
            continue;
        }

        // Method
        bool skip_params = attributes[ClassAttributes::GETTER] && !check(Token::Type::LEFT_PAREN);
        if (attributes[ClassAttributes::ABSTRACT]) {
            body.methods.emplace_back(abstract_method(member_name, skip_params), attributes);
        } else {
            body.methods.emplace_back(function_declaration_body(member_name, skip_params), attributes);
        }
    }
    consume(Token::Type::RIGHT_BRACE, "unmatched }");
    return body;
}

Stmt Parser::class_declaration(bool is_abstract) {
    consume(Token::Type::IDENTIFIER, "missing class name");
    Token class_name = current;

    std::optional<Token> superclass;
    if (match(Token::Type::COLON)) {
        consume(Token::Type::IDENTIFIER, "missing superclass name");
        superclass = current;
    }
    StructureBody body = structure_body();

    return ClassStmt {
            .name = class_name,
            .super_class = superclass,
            .body = std::move(body),
            .is_abstract = is_abstract
        };
}

ObjectExpr Parser::object_expression() {
    std::optional<Token> superclass;
    std::vector<Expr> superclass_arguments;
    if (match(Token::Type::COLON)) {
        consume(Token::Type::IDENTIFIER, "missing superclass name");
        superclass = current;

        if (match(Token::Type::LEFT_PAREN)) {
            superclass_arguments = consume_call_arguments();
        }
    }

    StructureBody body = structure_body();

    return ObjectExpr {
            .body = std::move(body),
            .super_class = superclass,
            .superclass_arguments = std::move(superclass_arguments)
        };
}

FunctionStmt Parser::in_trait_function(const Token& name, bool skip_params) {
    std::vector<Token> parameters = skip_params ? std::vector<Token>() : consume_functions_parameters();
    std::optional<Expr> body;
    if (match(Token::Type::LEFT_BRACE)) {
        body = block();
    }
    return FunctionStmt { .name = name, .params = std::move(parameters), .body = std::move(body) };
}

Stmt Parser::trait_declaration() {
    consume(Token::Type::IDENTIFIER, "missing trait name");
    Token trait_name = current;
    consume(Token::Type::LEFT_BRACE, "missing trait body");
    std::vector<FieldStmt> fields;
    std::vector<MethodStmt> methods;
    std::vector<UsingStmt> using_statements;
    while (!check(Token::Type::RIGHT_BRACE) && !check(Token::Type::LEFT_BRACE)) {
        if (match(Token::Type::USING)) {
            using_statements.push_back(using_statement());
            continue;
        }

        auto attributes = member_attributes(StructureType::TRAIT);
        consume(Token::Type::IDENTIFIER, "invalid trait member.");
        Token member_name = current;
        if (check(Token::Type::SEMICOLON)) {
            fields.emplace_back(abstract_field(member_name), attributes);
            continue;
        }

        bool skip_params = attributes[ClassAttributes::GETTER] && !check(Token::Type::LEFT_PAREN);
        methods.emplace_back(in_trait_function(member_name, skip_params), attributes);
    }

    consume(Token::Type::RIGHT_BRACE, "missing '}' after trait body");

    return TraitStmt {
            .name = trait_name,
            .methods = std::move(methods),
            .fields = std::move(fields),
            .using_stmts = std::move(using_statements)
        };
}

Stmt Parser::expr_statement() {
    Stmt stmt = ExprStmt { expression() };
    consume(Token::Type::SEMICOLON, "missing semicolon after expression");
    return stmt;
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
    return IfExpr {
            .condition = std::move(condition),
            .then_expr = std::move(then_stmt),
            .else_expr = std::move(else_stmt)
        };
}

Parser::Precedence Parser::get_precendece(Token::Type token) {
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

Expr Parser::super_() {
    consume(Token::Type::DOT, "missing '.' after 'super'");
    consume(Token::Type::IDENTIFIER, "missing superclass member identifier after 'super'");
    return SuperExpr { current };
}

// TODO: missing refactor
bool starts_block_expression(Token::Type type) {
    return type == Token::Type::LABEL || type == Token::Type::LEFT_BRACE || type == Token::Type::LOOP || type ==
        Token::Type::WHILE || type == Token::Type::FOR || type == Token::Type::IF;
}

// TODO: missing refactor
Expr Parser::block(std::optional<Token> label) {
    std::vector<Stmt> stmts;
    std::optional<Expr> expr_at_end = {};
    while (!check(Token::Type::RIGHT_BRACE) && !check(Token::Type::END)) {
        if (auto stmt = statement()) {
            stmts.push_back(std::move(*stmt));
        } else {
            bool is_block_expression = starts_block_expression(next.type);
            auto expr = expression();
            if (match(Token::Type::SEMICOLON) || (is_block_expression && (!check(Token::Type::RIGHT_BRACE) || current.
                type == Token::Type::SEMICOLON))) {
                stmts.push_back(ExprStmt(std::move(expr)));
            } else {
                expr_at_end = std::move(expr);
                break;
            }
        }
    }
    consume(Token::Type::RIGHT_BRACE, "Expected '}' after block.");
    return BlockExpr { .stmts = std::move(stmts), .expr = std::move(expr_at_end), .label = label };
}

Expr Parser::loop_expression(const std::optional<Token>& label) {
    consume(Token::Type::LEFT_BRACE, "missing 'loop' expression body");
    return LoopExpr(block(), label);
}

Expr Parser::break_expression() {
    std::optional<Token> label;
    if (match(Token::Type::LABEL)) {
        label = current;
    }
    if (!is_expression_start(next.type)) {
        return BreakExpr { .expr = {}, .label = label };
    }
    return BreakExpr { .expr = expression(), .label = label };
}

Expr Parser::continue_expression() {
    if (match(Token::Type::LABEL)) {
        return ContinueExpr(current);
    }
    return ContinueExpr();
}

Expr Parser::while_expression(const std::optional<Token>& label) {
    auto condition = expression();
    if (current.type != Token::Type::LEFT_BRACE) {
        error(current, "missing 'while' loop body", "Expected '{' here");
    }
    auto body = block();
    return WhileExpr(std::move(condition), std::move(body), label);
}

Expr Parser::labeled_expression() {
    Token label = current;
    consume(Token::Type::COLON, "missing colon after label");
    // all expressions we can break out of
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
    return InvalidExpr();
}

Expr Parser::grouping() {
    auto expr = expression();
    consume(Token::Type::RIGHT_PAREN, "unmatched ')'");
    return expr;
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
    return LiteralExpr { *result };
}

Expr Parser::number() {
    std::string literal = *current.string;
    std::expected<bite_float, ConversionError> result = string_to_floating(literal);
    if (!result) {
        error(current, result.error().what());
    }
    return LiteralExpr { *result };
}

Expr Parser::string() const {
    return StringLiteral { *current.string };
}

Expr Parser::identifier() {
    return VariableExpr { current };
}

Expr Parser::keyword() const {
    switch (current.type) {
        case Token::Type::NIL: return LiteralExpr { nil_t };
        case Token::Type::FALSE: return LiteralExpr { false };
        case Token::Type::TRUE: return LiteralExpr { true };
        case Token::Type::THIS: return ThisExpr {};
        default: std::unreachable();
    }
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

Expr Parser::unary(Token::Type operator_type) {
    return UnaryExpr { .expr = expression(Precedence::UNARY), .op = operator_type };
}

Expr Parser::dot(Expr left) {
    consume(Token::Type::IDENTIFIER, "missing identifier after dot"); // TODO: better error message
    return GetPropertyExpr { .left = std::move(left), .property = current };
}

Expr Parser::binary(Expr left) {
    Token::Type op = current.type;
    Precedence precedence = get_precendece(op);
    return BinaryExpr { std::move(left), expression(precedence), op };
}

Expr Parser::assigment(Expr left) {
    Token::Type op = current.type;
    Precedence precedence = get_precendece(op);
    // lower our precedence to handle right associativity
    // common technique in pratt parsers
    precedence = static_cast<Precedence>(static_cast<int>(precedence) - 1);
    return BinaryExpr { std::move(left), expression(precedence), op };
}

Expr Parser::call(Expr left) {
    std::vector<Expr> arguments = consume_call_arguments();
    return CallExpr { .callee = std::move(left), .arguments = std::move(arguments) };
}
