#include "Parser.h"

#include <cassert>
#include <iostream>

#include "conversions.h"

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
    return var_declaration_body(current);
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
    consume(Token::Type::LEFT_PAREN, "missing call arguments");
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
        super_arguments = consume_call_arguments();
    }

    consume(Token::Type::LEFT_BRACE, "missing constructor body");
    return ConstructorStmt {
            .parameters = parameters,
            .has_super = has_super,
            .super_arguments = std::move(super_arguments),
            .body = block()
        };
}

FunctionStmt Parser::abstract_method(Token name, bool skip_params) {
    std::vector<Token> parameters = skip_params ? std::vector<Token>() : consume_functions_parameters();
    consume(Token::Type::SEMICOLON, "missing semicolon after declaration");
    return FunctionStmt { .name = name, .params = std::move(parameters), .body = {} };
}

VarStmt Parser::abstract_field(Token name) {
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

Stmt Parser::trait_declaration() {
    consume(Token::Type::IDENTIFIER, "Expected trait name.");
    Token name = current;

    consume(Token::Type::LEFT_BRACE, "Expected '{' before trait body.");
    StructureMembers members = structure_body(StructureType::TRAIT);
    consume(Token::Type::RIGHT_BRACE, "Expected '}' after class body.");

    return TraitStmt {
            .name = name,
            .methods = std::move(members.methods),
            .fields = std::move(members.fields),
            .using_stmts = std::move(members.using_statements)
        };
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

Parser::StructureMembers Parser::structure_body(StructureType type) {
    // refactor: still kinda messy?
    StructureMembers members;
    while (!check(Token::Type::RIGHT_BRACE) && !check(Token::Type::END)) {
        if (check(Token::Type::OBJECT)) {
            advance();
            if (type == StructureType::OBJECT)
                error(current, "Class objects cannot be defined inside of objects.");
            members.class_object = object_expression();
            continue;
        }
        if (check(Token::Type::USING)) {
            advance();
            members.using_statements.push_back(using_statement());
            continue;
        }
        bitflags<ClassAttributes> attributes = member_attributes(type);
        consume(Token::Type::IDENTIFIER, "Expected identifier.");
        Token name = current;

        if (*name.string == "init") {
            if (type == StructureType::OBJECT)
                error(current, "Constructors cannot be defined inside of objects.");
            if (type == StructureType::TRAIT)
                error(current, "Constructors cannot be defined inside of traits.");
            // constructor call
            members.constructor = std::make_unique<ConstructorStmt>(constructor_statement());
        } else if ((check(Token::Type::SEMICOLON) || check(Token::Type::EQUAL)) && !attributes[ClassAttributes::SETTER]
            && !attributes[ClassAttributes::GETTER]) {
            attributes += ClassAttributes::GETTER;
            attributes += ClassAttributes::SETTER;
            if (type == StructureType::TRAIT) {
                attributes += ClassAttributes::ABSTRACT;
                if (!check(Token::Type::SEMICOLON))
                    error(current, "Expected ';' after trait declared field.");
                members.fields.emplace_back(abstract_field(name), attributes);
            } else {
                if (attributes[ClassAttributes::ABSTRACT]) {
                    members.fields.emplace_back(abstract_field(name), attributes);
                } else {
                    members.fields.emplace_back(var_declaration_after_name(name), attributes);
                }
            }
        } else {
            // TODO: validate get and setters function have expected number of arguments
            if (type == StructureType::TRAIT) {
                bool skip_params = attributes[ClassAttributes::GETTER] && !check(Token::Type::LEFT_PAREN);
                std::vector<Token> parameters;
                if (!skip_params) {
                    consume(Token::Type::LEFT_PAREN, "Expected '(' after function name");
                    if (!check(Token::Type::RIGHT_PAREN)) {
                        do {
                            consume(Token::Type::IDENTIFIER, "Expected identifier");
                            parameters.push_back(current);
                        } while (match(Token::Type::COMMA));
                    }
                    consume(Token::Type::RIGHT_PAREN, "Expected ')' after function parameters");
                }
                if (check(Token::Type::SEMICOLON)) {
                    attributes += ClassAttributes::ABSTRACT;
                    advance();
                    members.methods.emplace_back(FunctionStmt(name, std::move(parameters), {}), attributes);
                } else {
                    consume(Token::Type::LEFT_BRACE, "Expected '{' before function body");
                    members.methods.emplace_back(FunctionStmt(name, std::move(parameters), block()), attributes);
                }
            } else {
                if (attributes[ClassAttributes::ABSTRACT]) {
                    bool skip_params = attributes[ClassAttributes::GETTER];
                    members.methods.emplace_back(FunctionStmt(abstract_method(current, skip_params)), attributes);
                } else {
                    bool skip_params = attributes[ClassAttributes::GETTER] && !check(Token::Type::LEFT_PAREN);
                    members.methods.emplace_back(
                        FunctionStmt(function_declaration_body(current, skip_params)),
                        attributes
                    );
                }
            }
        }
    }
    return std::move(members);
}

ObjectExpr Parser::object_expression() {
    // superclass list
    std::optional<Token> superclass;
    std::vector<Expr> superclass_arguments;
    if (match(Token::Type::COLON)) {
        consume(Token::Type::IDENTIFIER, "Expected superclass name.");
        superclass = current;

        if (match(Token::Type::LEFT_PAREN)) {
            superclass_arguments = arguments_list();
        }
    }

    consume(Token::Type::LEFT_BRACE, "Expected '{' after object body");
    StructureMembers members = structure_body(StructureType::OBJECT);
    consume(Token::Type::RIGHT_BRACE, "Expected '}' after object body.");
    return ObjectExpr {
            .methods = std::move(members.methods),
            .fields = std::move(members.fields),
            .super_class = superclass,
            .superclass_arguments = std::move(superclass_arguments),
            .using_stmts = std::move(members.using_statements)
        };
}

Stmt Parser::class_declaration(bool is_abstract) {
    consume(Token::Type::IDENTIFIER, "Expected class name.");
    Token name = current;

    std::optional<Token> super_class;
    if (match(Token::Type::COLON)) {
        consume(Token::Type::IDENTIFIER, "Expected superclass name.");
        super_class = current;
    }
    consume(Token::Type::LEFT_BRACE, "Expected '{' before class body.");
    StructureMembers members = structure_body(is_abstract ? StructureType::ABSTRACT_CLASS : StructureType::CLASS);
    consume(Token::Type::RIGHT_BRACE, "Expected '}' after class body.");

    return ClassStmt {
            .name = name,
            .constructor = std::move(members.constructor),
            .methods = std::move(members.methods),
            .fields = std::move(members.fields),
            .class_object = std::move(members.class_object),
            .super_class = super_class,
            .is_abstract = is_abstract,
            .using_statements = std::move(members.using_statements)
        };
}

Stmt Parser::expr_statement() {
    Stmt stmt = ExprStmt { expression() };
    consume(Token::Type::SEMICOLON, "Expected ';' after expression");
    return stmt;
}

Expr Parser::if_expression() {
    auto condition = expression();
    if (current.type != Token::Type::LEFT_BRACE) {
        error(current, "Expected '{' after 'if' condition.");
    }
    auto then_stmt = block();
    std::optional<Expr> else_stmt = {};
    if (match(Token::Type::ELSE)) {
        if (match(Token::Type::IF)) {
            else_stmt = if_expression();
        } else {
            consume(Token::Type::LEFT_BRACE, "Expected '{' after 'else' keyword.");
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
        error(current, "Expected expression.");
        // TODO: recover!!!
        //return {};
    }
    while (precedence < get_precendece(next.type)) {
        advance();
        left = infix(std::move(*left));
    }
    return std::move(*left);
}

Expr Parser::this_() {
    return ThisExpr {};
}

Expr Parser::super_() {
    consume(Token::Type::DOT, "Expected '.' after 'super'.");
    consume(Token::Type::IDENTIFIER, "Expected superclass method name.");
    return SuperExpr { current };
}


bool starts_block_expression(Token::Type type) {
    return type == Token::Type::LABEL || type == Token::Type::LEFT_BRACE || type == Token::Type::LOOP || type ==
        Token::Type::WHILE || type == Token::Type::FOR || type == Token::Type::IF;
}

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

Expr Parser::loop_expression(std::optional<Token> label) {
    consume(Token::Type::LEFT_BRACE, "Expected '{' after loop keyword.");
    return LoopExpr(block(), label);
}

Expr Parser::break_expression() {
    std::optional<Token> label;
    if (match(Token::Type::LABEL)) {
        label = current;
    }
    if (!has_prefix(next.type)) {
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

Expr Parser::while_expression(std::optional<Token> label) {
    auto condition = expression();
    if (current.type != Token::Type::LEFT_BRACE) {
        error(current, "Expected '{' after while loop condition");
    }
    auto body = block();
    return WhileExpr(std::move(condition), std::move(body), label);
}

Expr Parser::labeled_expression() {
    Token label = current;
    consume(Token::Type::COLON, "Expected ':' after label");
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
    error(label, "Only loops and blocks can be labeled.");
    // TODO: recover!
    // return {};
}


std::optional<Expr> Parser::prefix() {
    switch (current.type) {
        case Token::Type::INTEGER: return integer();
        case Token::Type::NUMBER: return number();
        case Token::Type::STRING: return string();
        case Token::Type::TRUE:
        case Token::Type::FALSE:
        case Token::Type::NIL: return keyword();
        case Token::Type::IDENTIFIER: return identifier();
        case Token::Type::LEFT_PAREN: return grouping();
        case Token::Type::LEFT_BRACE: return block();
        case Token::Type::BANG:
        case Token::Type::MINUS:
        case Token::Type::TILDE: return unary(current.type);
        case Token::Type::THIS: return this_();
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
        default: assert(false); // unreachable!
    }
}

Expr Parser::grouping() {
    auto expr = expression();
    consume(Token::Type::RIGHT_PAREN, "Expected ')' after expression.");
    return expr;
}

Expr Parser::unary(Token::Type operator_type) {
    return UnaryExpr { .expr = expression(Precedence::UNARY), .op = operator_type };
}

Expr Parser::dot(Expr left) {
    consume(Token::Type::IDENTIFIER, "Expected property name after '.'");
    return GetPropertyExpr { .left = std::move(left), .property = current };
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
        case Token::Type::BAR_EQUAL: return binary(std::move(left), true);
        case Token::Type::LEFT_PAREN: return call(std::move(left));
        case Token::Type::DOT: return dot(std::move(left));
        default: return left;
    }
}

Expr Parser::binary(Expr left, bool expect_lvalue) {
    if (expect_lvalue && !std::holds_alternative<bite::box<VariableExpr>>(left) && !std::holds_alternative<bite::box<
        GetPropertyExpr>>(left) && !std::holds_alternative<bite::box<SuperExpr>>(left)) {
        error(current, "Expected lvalue as left hand side of an binary expression.");
    }
    Token::Type op = current.type;
    Precedence precedence = get_precendece(op);
    // handle right-associativity
    if (precedence == Precedence::ASSIGMENT) {
        precedence = static_cast<Precedence>(static_cast<int>(precedence) - 1);
    }
    return BinaryExpr { std::move(left), expression(precedence), op };
}

Expr Parser::call(Expr left) {
    std::vector<Expr> arguments;
    if (!check(Token::Type::RIGHT_PAREN)) {
        do {
            arguments.push_back(expression());
        } while (match(Token::Type::COMMA));
    }
    consume(Token::Type::RIGHT_PAREN, "Expected ')' after call arguments.");
    return CallExpr { .callee = std::move(left), .arguments = std::move(arguments) };
}
