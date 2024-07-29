#include "Parser.h"

#include <cassert>
#include <iostream>

#include "conversions.h"

void Parser::error(const Token &token, std::string_view message) {
    if (panic_mode) return;
    panic_mode = true;
    errors.emplace_back(token, message);
}

void Parser::synchronize() {
    if (!panic_mode) return;
    panic_mode = false;
    while (!check(Token::Type::END)) {
        if (current.type == Token::Type::SEMICOLON) return;

        // synchonization points (https://www.ssw.uni-linz.ac.at/Misc/CC/slides/03.Parsing.pdf)
        // should synchronize on start of statement or declaration
        switch (next.type) {
            case Token::Type::LET:
            case Token::Type::LEFT_BRACE:
            case Token::Type::IF:
            case Token::Type::WHILE:
            case Token::Type::FUN:
            case Token::Type::RETURN:
                return;
            default: advance();
        }
    }
}

const std::vector<Parser::Error> &Parser::get_errors() {
    return errors;
}

Token Parser::advance() {
    current = next;
    while (true) {
        auto token = lexer.next_token();
        if (!token) {
            error(current, token.error().message);
        } else {
            next = *token;
            break;
        }
    }
    return next;
}

bool Parser::check(const Token::Type type) const {
    return next.type == type;
}


void Parser::consume(const Token::Type type, const std::string_view message) {
    if (check(type)) {
        advance();
        return;
    }
    error(current, message);
}

bool Parser::match(Token::Type type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

std::vector<Stmt> Parser::parse() {
    advance(); // populate next
    std::vector<Stmt> stmts;
    while (!match(Token::Type::END)) {
        stmts.push_back(statement_or_expression());
    }
    return stmts;
}

Stmt Parser::statement_or_expression() {
    if (auto stmt = statement()) {
        return std::move(*stmt);
    }
    if (match(Token::Type::IF)) {
        auto expr = if_expression();
        match(Token::Type::SEMICOLON);
        return ExprStmt(std::make_unique<Expr>(std::move(expr)));
    }
    if (match(Token::Type::LOOP)) {
        auto expr = loop_expression();
        match(Token::Type::SEMICOLON);
        return ExprStmt(std::make_unique<Expr>(std::move(expr)));
    }
    if (match(Token::Type::WHILE)) {
        auto expr = while_expression();
        match(Token::Type::SEMICOLON);
        return ExprStmt(std::make_unique<Expr>(std::move(expr)));
    }
    if (match(Token::Type::FOR)) {
        auto expr = for_expression();
        match(Token::Type::SEMICOLON);
        return ExprStmt(std::make_unique<Expr>(std::move(expr)));
    }
    if (match(Token::Type::LABEL)) {
        auto expr = labeled_expression();
        match(Token::Type::SEMICOLON);
        return ExprStmt(std::make_unique<Expr>(std::move(expr)));
    }
    if (match(Token::Type::RETURN)) {
        auto expr = return_expression();
        match(Token::Type::SEMICOLON);
        return ExprStmt(std::make_unique<Expr>(std::move(expr)));
    }
    auto expr = std::make_unique<Expr>(expression());
    consume(Token::Type::SEMICOLON, "Expected ';' after expression.");
    return ExprStmt(std::move(expr));
}

Stmt Parser::native_declaration() {
    consume(Token::Type::IDENTIFIER, "Expected identifier after 'native' keyword.");
    Token name = current;
    consume(Token::Type::SEMICOLON, "Expected ';' after native declaration.");
    return NativeStmt(name);
}

Expr Parser::for_expression(std::optional<Token> label) {
    consume(Token::Type::IDENTIFIER, "Expected an identifier after 'for'.");
    Token name = current;
    consume(Token::Type::IN, "Expected 'in' after item name in for loop.");
    Expr iterable = expression();
    if (current.type != Token::Type::LEFT_BRACE) {
        error(current, "Expected '{' before loop body.");
    }
    Expr body = block();
    return ForExpr{
        .name = name,
        .iterable = make_expr_handle(std::move(iterable)),
        .body = make_expr_handle(std::move(body)),
        .label = label
    };
}

Expr Parser::return_expression() {
    ExprHandle expr = nullptr;
    if (!match(Token::Type::SEMICOLON)) {
        expr = make_expr_handle(expression());
        consume(Token::Type::SEMICOLON, "Exepected ';' after return value.");
    }
    return ReturnExpr{std::move(expr)};
}

Stmt Parser::object_declaration() {
    consume(Token::Type::IDENTIFIER, "Expected object name.");
    Token name = current;
    return ObjectStmt {
        .name = name,
        .object = make_expr_handle(object_expression())
    };
}

std::optional<Stmt> Parser::statement() {
    auto exit = scope_exit([this] { if (panic_mode) synchronize(); });

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
        consume(Token::Type::CLASS, "Expected 'class' after 'abstract'.");
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
    consume(Token::Type::IDENTIFIER, "expected identifier");
    return var_declaration_after_name(current);
}

VarStmt Parser::var_declaration_after_name(const Token name) {
    Expr expr = match(Token::Type::EQUAL) ? expression() : LiteralExpr{nil_t};
    consume(Token::Type::SEMICOLON, "Expected ';' after variable declaration.");
    return VarStmt{.name = name, .value = make_expr_handle(std::move(expr))};
}

FunctionStmt Parser::function_declaration() {
    consume(Token::Type::IDENTIFIER, "expected function name");
    return function_declaration_after_name(current);
}

FunctionStmt Parser::function_declaration_after_name(const Token name, bool skip_params) {
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
    consume(Token::Type::LEFT_BRACE, "Expected '{' before function body");
    auto body = block();
    return FunctionStmt{
        .name = name,
        .params = std::move(parameters),
        .body = std::make_unique<Expr>(std::move(body))
    };
}

ConstructorStmt Parser::constructor_statement() {
    // overlap with function declaration!
    consume(Token::Type::LEFT_PAREN, "Expected '(' after constructor name");
    std::vector<Token> parameters;
    if (!check(Token::Type::RIGHT_PAREN)) {
        do {
            consume(Token::Type::IDENTIFIER, "Expected identifier");
            parameters.push_back(current);
        } while (match(Token::Type::COMMA));
    }
    consume(Token::Type::RIGHT_PAREN, "Expected ')' after constructor parameters");

    std::vector<ExprHandle> super_arguments;
    bool has_super = false;
    // init(parameters*) : super(arguments*) [block]
    if (match(Token::Type::COLON)) {
        has_super = true;
        consume(Token::Type::SUPER, "Expected superclass constructor call after ':' ");
        // overlap with function call!!!
        consume(Token::Type::LEFT_PAREN, "Expected '(' after superclass constructor call");
        if (!check(Token::Type::RIGHT_PAREN)) {
            do {
                super_arguments.emplace_back(make_expr_handle(expression()));
            } while (match(Token::Type::COMMA));
        }
        consume(Token::Type::RIGHT_PAREN, "Expected ')' after superclass constructor call arguments.");
    }

    consume(Token::Type::LEFT_BRACE, "Expected '{' before constructor body");
    return ConstructorStmt{
        .parameters = parameters,
        .has_super = has_super,
        .super_arguments = std::move(super_arguments),
        .body = make_expr_handle(block())
    };
}

FunctionStmt Parser::abstract_method(Token name, bool skip_params) {
    // overlap with function!
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
    consume(Token::Type::SEMICOLON, "Expected ';' after abstract function declaration");
    return FunctionStmt{
        .name = name,
        .params = std::move(parameters),
        .body = nullptr
    };
}

VarStmt Parser::abstract_field(Token name) {
    Expr expr = match(Token::Type::EQUAL) ? expression() : LiteralExpr{nil_t};
    consume(Token::Type::SEMICOLON, "Expected ';' after variable declaration.");
    return VarStmt{.name = name, .value = nullptr};
}

// refactor: use across whole parser
std::vector<ExprHandle> Parser::arguments_list() {
    std::vector<ExprHandle> arguments;
    do {
        arguments.emplace_back(make_expr_handle(expression()));
    } while (match(Token::Type::COMMA));
    consume(Token::Type::RIGHT_PAREN, "Expected ')' after arguments.");
    return std::move(arguments);
}

bitflags<ClassAttributes> Parser::member_attributes(StructureType outer_type) {
    // TODO: better error reporting. current setup relies too much on good order of atributes
    bitflags<ClassAttributes> attributes;
    if (match(Token::Type::PRIVATE)) {
        attributes += ClassAttributes::PRIVATE;
    }
    if (match(Token::Type::OVERRDIE)) {
        if (attributes[ClassAttributes::PRIVATE]) error(current, "Overrides cannot be private");
        attributes += ClassAttributes::OVERRIDE;
    }
    if (match(Token::Type::ABSTRACT)) {
        if (outer_type != StructureType::ABSTRACT_CLASS) {
            error(current, "Abstract methods can be only declared in abstract classes.");
        }
        if (attributes[ClassAttributes::PRIVATE]) error(current, "Abstract members cannot be private");
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
            if (type == StructureType::OBJECT) error(current, "Class objects cannot be defined inside of objects.");
            members.class_object = make_expr_handle(object_expression());
            continue;
        }
        if (check(Token::Type::USING)) {
            advance();
            members.using_statements.push_back(std::make_unique<UsingStmt>(using_statement()));
            continue;
        }
        bitflags<ClassAttributes> attributes = member_attributes(type);
        consume(Token::Type::IDENTIFIER, "Expected identifier.");
        Token name = current;

        if (name.get_lexeme(lexer.get_source()) == "init") {
            if (type == StructureType::OBJECT) error(current, "Constructors cannot be defined inside of objects.");
            if (type == StructureType::TRAIT) error(current, "Constructors cannot be defined inside of traits.");
            // constructor call
            members.constructor = std::make_unique<ConstructorStmt>(constructor_statement());
        } else if ((check(Token::Type::SEMICOLON) || check(Token::Type::EQUAL)) && !attributes[ClassAttributes::SETTER]
                   && !attributes[ClassAttributes::GETTER]) {
            attributes += ClassAttributes::GETTER;
            attributes += ClassAttributes::SETTER;
            if (type == StructureType::TRAIT) {
                attributes += ClassAttributes::ABSTRACT;
                if (!check(Token::Type::SEMICOLON)) error(current, "Expected ';' after trait declared field.");
                members.fields.push_back(std::make_unique<FieldStmt>(
                        std::make_unique<VarStmt>(abstract_field(name)), attributes
                    ));
            } else {
                if (attributes[ClassAttributes::ABSTRACT]) {
                    members.fields.push_back(std::make_unique<FieldStmt>(
                        std::make_unique<VarStmt>(abstract_field(name)), attributes
                    ));
                } else {
                    members.fields.push_back(std::make_unique<FieldStmt>(
                        std::make_unique<VarStmt>(var_declaration_after_name(name)), attributes
                    ));
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
                    members.methods.push_back(std::make_unique<MethodStmt>(std::make_unique<FunctionStmt>(name, std::move(parameters), nullptr), attributes));
                } else {
                    consume(Token::Type::LEFT_BRACE, "Expected '{' before function body");
                    members.methods.push_back(std::make_unique<MethodStmt>(std::make_unique<FunctionStmt>(name, std::move(parameters), make_expr_handle(block())), attributes));

                }
            } else {
                if (attributes[ClassAttributes::ABSTRACT]) {
                    bool skip_params = attributes[ClassAttributes::GETTER];
                    members.methods.push_back(std::make_unique<MethodStmt>(
                        std::make_unique<FunctionStmt>(abstract_method(current, skip_params)),
                        attributes
                    ));
                } else {
                    bool skip_params = attributes[ClassAttributes::GETTER] && !check(Token::Type::LEFT_PAREN);
                    members.methods.push_back(std::make_unique<MethodStmt>(
                        std::make_unique<FunctionStmt>(function_declaration_after_name(current, skip_params)),
                        attributes
                    ));
                }
            }
        }
    }
    return std::move(members);
}

ObjectExpr Parser::object_expression() {
    // superclass list
    std::optional<Token> superclass;
    std::vector<ExprHandle> superclass_arguments;
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
    Stmt stmt = ExprStmt{make_expr_handle(expression())};
    consume(Token::Type::SEMICOLON, "Expected ';' after expression");
    return stmt;
}

Expr Parser::if_expression() {
    auto condition = expression();
    if (current.type != Token::Type::LEFT_BRACE) {
        error(current, "Expected '{' after 'if' condition.");
    }
    auto then_stmt = block();
    ExprHandle else_stmt = nullptr;
    if (match(Token::Type::ELSE)) {
        if (match(Token::Type::IF)) {
            else_stmt = make_expr_handle(if_expression());
        } else {
            consume(Token::Type::LEFT_BRACE, "Expected '{' after 'else' keyword.");
            else_stmt = make_expr_handle(block());
        }
    }
    return IfExpr{
        .condition = make_expr_handle(std::move(condition)),
        .then_expr = make_expr_handle(std::move(then_stmt)),
        .else_expr = std::move(else_stmt)
    };
}

Parser::Precedence Parser::get_precendece(Token::Type token) {
    switch (token) {
        case Token::Type::PLUS:
        case Token::Type::MINUS:
            return Precedence::TERM;
        case Token::Type::STAR:
        case Token::Type::SLASH:
        case Token::Type::SLASH_SLASH:
        case Token::Type::PERCENT:
            return Precedence::FACTOR;
        case Token::Type::EQUAL_EQUAL:
        case Token::Type::BANG_EQUAL:
            return Precedence::EQUALITY;
        case Token::Type::LESS:
        case Token::Type::LESS_EQUAL:
        case Token::Type::GREATER:
        case Token::Type::GREATER_EQUAL:
            return Precedence::RELATIONAL;
        case Token::Type::LESS_LESS:
        case Token::Type::GREATER_GREATER:
            return Precedence::BITWISE_SHIFT;
        case Token::Type::AND:
            return Precedence::BITWISE_AND;
        case Token::Type::BAR:
            return Precedence::BITWISE_OR;
        case Token::Type::CARET:
            return Precedence::BITWISE_XOR;
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
        case Token::Type::BAR_EQUAL:

            return Precedence::ASSIGMENT;
        case Token::Type::AND_AND:
            return Precedence::LOGICAL_AND;
        case Token::Type::BAR_BAR:
            return Precedence::LOGICAL_OR;
        case Token::Type::LEFT_PAREN:
        case Token::Type::LEFT_BRACE:
        case Token::Type::DOT:
            return Precedence::CALL;
        case Token::Type::IF:
        case Token::Type::LOOP:
        case Token::Type::WHILE:
        case Token::Type::FOR:
        case Token::Type::LABEL:
        case Token::Type::RETURN:
        case Token::Type::OBJECT:
        default:
            return Precedence::NONE;
    }
}

Expr Parser::expression(const Precedence precedence) {
    advance();
    auto left = prefix();
    if (!left) {
        error(current, "Expected expression.");
        return {};
    }
    while (precedence < get_precendece(next.type)) {
        advance();
        left = infix(std::move(*left));
    }
    return std::move(*left);
}

Expr Parser::this_() {
    return ThisExpr{};
}

Expr Parser::super_() {
    consume(Token::Type::DOT, "Expected '.' after 'super'.");
    consume(Token::Type::IDENTIFIER, "Expected superclass method name.");
    return SuperExpr{current};
}

bool has_prefix(Token::Type type) {
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
        case Token::Type::RETURN:
            return true;
        default: return false;
    }
}

bool starts_block_expression(Token::Type type) {
    return type == Token::Type::LABEL
           || type == Token::Type::LEFT_BRACE
           || type == Token::Type::LOOP
           || type == Token::Type::WHILE
           || type == Token::Type::FOR
           || type == Token::Type::IF;
}

Expr Parser::block(std::optional<Token> label) {
    std::vector<StmtHandle> stmts;
    ExprHandle expr_at_end = nullptr;
    while (!check(Token::Type::RIGHT_BRACE) && !check(Token::Type::END)) {
        if (auto stmt = statement()) {
            stmts.push_back(std::make_unique<Stmt>(std::move(*stmt)));
        } else {
            bool is_block_expression = starts_block_expression(next.type);
            auto expr = expression();
            if (match(Token::Type::SEMICOLON) || (
                    is_block_expression && (!check(Token::Type::RIGHT_BRACE) || current.type ==
                                            Token::Type::SEMICOLON))) {
                stmts.push_back(std::make_unique<Stmt>(ExprStmt(std::make_unique<Expr>(std::move(expr)))));
            } else {
                expr_at_end = std::make_unique<Expr>(std::move(expr));
                break;
            }
        }
    }
    consume(Token::Type::RIGHT_BRACE, "Expected '}' after block.");
    return BlockExpr{.stmts = std::move(stmts), .expr = std::move(expr_at_end), .label = label};
}

Expr Parser::loop_expression(std::optional<Token> label) {
    consume(Token::Type::LEFT_BRACE, "Expected '{' after loop keyword.");
    return LoopExpr(make_expr_handle(block()), label);
}

Expr Parser::break_expression() {
    std::optional<Token> label;
    if (match(Token::Type::LABEL)) {
        label = current;
    }
    if (!has_prefix(next.type)) {
        return BreakExpr{.label = label};
    }
    return BreakExpr{make_expr_handle(expression()), label};
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
    return WhileExpr(make_expr_handle(std::move(condition)), make_expr_handle(std::move(body)), label);
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
    return {};
}


std::optional<Expr> Parser::prefix() {
    switch (current.type) {
        case Token::Type::INTEGER:
            return integer();
        case Token::Type::NUMBER:
            return number();
        case Token::Type::STRING:
            return string();
        case Token::Type::TRUE:
        case Token::Type::FALSE:
        case Token::Type::NIL:
            return keyword();
        case Token::Type::IDENTIFIER:
            return identifier();
        case Token::Type::LEFT_PAREN:
            return grouping();
        case Token::Type::LEFT_BRACE:
            return block();
        case Token::Type::BANG:
        case Token::Type::MINUS:
        case Token::Type::TILDE:
            return unary(current.type);
        case Token::Type::THIS:
            return this_();
        case Token::Type::SUPER:
            return super_();
        case Token::Type::IF:
            return if_expression();
        case Token::Type::LOOP:
            return loop_expression();
        case Token::Type::BREAK:
            return break_expression();
        case Token::Type::CONTINUE:
            return continue_expression();
        case Token::Type::WHILE:
            return while_expression();
        case Token::Type::FOR:
            return for_expression();
        case Token::Type::LABEL:
            return labeled_expression();
        case Token::Type::RETURN:
            return return_expression();
        case Token::Type::OBJECT:
            return object_expression();
        default: return {};
    }
}

Expr Parser::integer() {
    std::string literal = current.get_lexeme(lexer.get_source());
    std::expected<bite_int, ConversionError> result = string_to_int(literal);
    if (!result) {
        error(current, result.error().what());
    }
    return LiteralExpr{*result};
}

Expr Parser::number() {
    std::string literal = current.get_lexeme(lexer.get_source());
    std::expected<bite_float, ConversionError> result = string_to_floating(literal);
    if (!result) {
        error(current, result.error().what());
    }
    return LiteralExpr{*result};
}

Expr Parser::string() const {
    return StringLiteral{std::string(current.get_lexeme(lexer.get_source()))};
}

Expr Parser::identifier() {
    return VariableExpr{current};
}

Expr Parser::keyword() const {
    switch (current.type) {
        case Token::Type::NIL:
            return LiteralExpr{nil_t};
        case Token::Type::FALSE:
            return LiteralExpr{false};
        case Token::Type::TRUE:
            return LiteralExpr{true};
        default: assert(false); // unreachable!
    }
}

Expr Parser::grouping() {
    auto expr = expression();
    consume(Token::Type::RIGHT_PAREN, "Expected ')' after expression.");
    return expr;
}

Expr Parser::unary(Token::Type operator_type) {
    return UnaryExpr{.expr = make_expr_handle(expression(Precedence::UNARY)), .op = operator_type};
}

Expr Parser::dot(Expr left) {
    consume(Token::Type::IDENTIFIER, "Expected property name after '.'");
    return GetPropertyExpr{.left = make_expr_handle(std::move(left)), .property = current};
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
        case Token::Type::PERCENT:
            return binary(std::move(left));
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
        case Token::Type::BAR_EQUAL:
            return binary(std::move(left), true);
        case Token::Type::LEFT_PAREN:
            return call(std::move(left));
        case Token::Type::DOT:
            return dot(std::move(left));
        default:
            return left;
    }
}

Expr Parser::binary(Expr left, bool expect_lvalue) {
    if (expect_lvalue && !std::holds_alternative<VariableExpr>(left) && !std::holds_alternative<
            GetPropertyExpr>(left) && !std::holds_alternative<SuperExpr>(left)) {
        error(current, "Expected lvalue as left hand side of an binary expression.");
    }
    Token::Type op = current.type;
    Precedence precedence = get_precendece(op);
    // handle right-associativity
    if (precedence == Precedence::ASSIGMENT) {
        precedence = static_cast<Precedence>(static_cast<int>(precedence) - 1);
    }
    return BinaryExpr{
        make_expr_handle(std::move(left)),
        make_expr_handle(expression(precedence)), op
    };
}

Expr Parser::call(Expr left) {
    std::vector<ExprHandle> arguments;
    if (!check(Token::Type::RIGHT_PAREN)) {
        do {
            arguments.push_back(make_expr_handle(expression()));
        } while (match(Token::Type::COMMA));
    }
    consume(Token::Type::RIGHT_PAREN, "Expected ')' after call arguments.");
    return CallExpr{.callee = make_expr_handle(std::move(left)), .arguments = std::move(arguments)};
}
