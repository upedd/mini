#include "Parser.h"

#include <experimental/scope>

#include "conversions.h"
#include "../Object.h"
#include "../shared/SharedContext.h"

void Parser::error(const Token& token, const std::string& message, const std::string& inline_message) {
    // TODO: temporary
    if (panic_mode) {
        return;
    }
    panic_mode = true;
    m_has_errors = true;
    context->diagnostics.add(
        bite::Diagnostic {
            .level = bite::DiagnosticLevel::ERROR,
            .message = message,
            .inline_hints = {
                bite::InlineHint {
                    .location = token.span,
                    .message = inline_message,
                    .level = bite::DiagnosticLevel::ERROR
                }
            }
        }
    );
}

void Parser::warning(const Token& token, const std::string& message, const std::string& inline_message) {
    // TODO: temporary
    context->diagnostics.add(
        bite::Diagnostic {
            .level = bite::DiagnosticLevel::ERROR,
            .message = message,
            .inline_hints = {
                bite::InlineHint {
                    .location = token.span,
                    .message = inline_message,
                    .level = bite::DiagnosticLevel::ERROR
                }
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
    for (auto& span : span_stack) {
        span.merge(next.span);
    }
    while (true) {
        if (auto token = lexer.next_token()) {
            next = *token;
            break;
        } else {
            context->diagnostics.add(token.error());
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

std::unique_ptr<Stmt> Parser::import_stmt() {
    std::vector<std::unique_ptr<ImportStmt::Item>> items;
    do {
        auto expr = expression();
        Token name;
        if (expr->is_variable_expr()) {
            name = expr->as_variable_expr()->identifier;
        } else if (expr->is_module_resolution_expr()) {
            name = expr->as_module_resolution_expr()->path.back();
        } else {
            // better error?
            error(current, "import item must be either a identifer of path resolution expression");
        }
        if (match(Token::Type::AS)) {
            consume(Token::Type::IDENTIFIER, "missing import aliasas");
            name = current;
        }
        items.emplace_back(std::make_unique<ImportStmt::Item>(current.span, name, std::move(expr)));
    } while (match(Token::Type::COMMA));
    consume(Token::Type::FROM, "import does not specify destination");
    // advance();
    //consume(Token::Type::IDENTIFIER, "expected module name");
    // TODO!!!
    auto stmt = std::make_unique<ImportStmt>(make_span(), std::move(items), expression());
    consume(Token::Type::SEMICOLON, "missing semicolon after import");
    return stmt;
}

std::unique_ptr<ModuleStmt> Parser::module_stmt() {
    consume(Token::Type::IDENTIFIER, "missing module name");
    Token module_name = current;
    consume(Token::Type::LEFT_BRACE, "missing module body");
    std::vector<std::unique_ptr<Stmt>> stmts;
    while (!match(Token::Type::RIGHT_BRACE)) {
        if (auto stmt = statement()) {
            stmts.emplace_back(std::move(*stmt));
        } else {
            error(current, "only declarations are allowed inside of modules", "is not a declaration");
        }
    }
    return std::make_unique<ModuleStmt>(make_span(), module_name, std::move(stmts));
}

Ast Parser::parse() {
    advance(); // populate next
    std::vector<std::unique_ptr<Stmt>> stmts;
    while (!match(Token::Type::END)) {
        stmts.emplace_back(statement_or_expression());
    }
    return Ast(std::move(stmts));
}

std::unique_ptr<Stmt> Parser::statement_or_expression() {
    auto handle = std::experimental::scope_exit(
        [this] {
            if (panic_mode) {
                synchronize();
            }
        }
    );

    if (auto stmt = statement()) {
        return std::move(*stmt);
    }
    if (auto stmt = control_flow_expression_statement()) {
        return std::move(*stmt);
    }
    return expr_statement();
}

std::optional<std::unique_ptr<Stmt>> Parser::statement() {
    return with_source_span(
        [this] -> std::optional<std::unique_ptr<Stmt>> {
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
            if (match(Token::Type::OBJECT)) {
                return object_declaration();
            }
            if (match(Token::Type::TRAIT)) {
                return trait_declaration();
            }
            if (match(Token::Type::IMPORT)) {
                return import_stmt();
            }
            if (match(Token::Type::MODULE)) {
                return module_stmt();
            }
            return {};
        }
    );
}

std::optional<std::unique_ptr<Stmt>> Parser::control_flow_expression_statement() {
    return with_source_span(
        [this] -> std::optional<std::unique_ptr<Stmt>> {
            std::optional<std::unique_ptr<Expr>> expr;
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
            return std::make_unique<ExprStmt>(make_span(), std::move(*expr));
        }
    );
}

std::unique_ptr<Stmt> Parser::expr_statement() {
    return with_source_span(
        [this] {
            std::unique_ptr<Stmt> stmt = std::make_unique<ExprStmt>(make_span(), expression());
            consume(Token::Type::SEMICOLON, "missing semicolon after expression");
            return stmt;
        }
    );
}

std::unique_ptr<ObjectDeclaration> Parser::object_declaration() {
    consume(Token::Type::IDENTIFIER, "missing object name");
    Token name = current;
    return std::make_unique<ObjectDeclaration>(make_span(), name, object_expression());
}

std::unique_ptr<VariableDeclaration> Parser::var_declaration() {
    consume(Token::Type::IDENTIFIER, "missing variable name");
    Token name = current;
    return var_declaration_body(name);
}

// The part after name
std::unique_ptr<VariableDeclaration> Parser::var_declaration_body(const Token& name) {
    std::unique_ptr<Expr> expr = match(Token::Type::EQUAL)
                                     ? expression()
                                     : std::make_unique<LiteralExpr>(make_span(), nil_t);
    consume(Token::Type::SEMICOLON, "missing semicolon");
    return std::make_unique<VariableDeclaration>(make_span(), name, std::move(expr));
}

std::unique_ptr<FunctionDeclaration> Parser::function_declaration() {
    consume(Token::Type::IDENTIFIER, "missing function name");
    Token name = current;
    return function_declaration_body(name);
}

// The part after name
std::unique_ptr<FunctionDeclaration> Parser::function_declaration_body(const Token& name, const bool skip_params) {
    std::vector<Token> parameters = skip_params ? std::vector<Token>() : functions_parameters();
    consume(Token::Type::LEFT_BRACE, "Expected '{' before function body");
    auto body = block();
    return std::make_unique<FunctionDeclaration>(make_span(), name, std::move(parameters), std::move(body));
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

std::vector<std::unique_ptr<Expr>> Parser::call_arguments() {
    std::vector<std::unique_ptr<Expr>> arguments;
    if (!check(Token::Type::RIGHT_PAREN)) {
        do {
            arguments.push_back(expression());
        } while (match(Token::Type::COMMA));
    }
    consume(Token::Type::RIGHT_PAREN, "unmatched ')'");
    return arguments;
}

TraitUsage Parser::trait_usage() {
    return with_source_span(
        [this] {
            consume(Token::Type::IDENTIFIER, "expected trait name");
            Token trait = current;
            std::vector<Token> exclusions;
            std::vector<std::pair<Token, Token>> aliases;
            if (match(Token::Type::LEFT_PAREN)) {
                do {
                    if (match(Token::Type::EXCLUDE)) {
                        consume(Token::Type::IDENTIFIER, "invalid exclusion item");
                        exclusions.push_back(current);
                    } else {
                        consume(Token::Type::IDENTIFIER, "invalid trait composition argument");
                        Token before = current;
                        consume(Token::Type::AS, "invalid trait composition argument");
                        consume(Token::Type::IDENTIFIER, "invalid alias");
                        Token after = current;
                        aliases.emplace_back(before, after);
                    }
                } while (match(Token::Type::COMMA));
                consume(Token::Type::RIGHT_PAREN, "unmatched ')'");
            }

            return TraitUsage {
                    .trait = trait,
                    .exclusions = std::move(exclusions),
                    .aliases = std::move(aliases),
                    .span = make_span()
                };
        }
    );
}

Constructor Parser::constructor() {
    return with_source_span(
        [this] {
            Token init_token = advance();

            std::vector<Token> parameters = functions_parameters();
            std::optional<SuperConstructorCall> super_constructor_call;
            // init(parameters*) : super(arguments*) [block]
            if (match(Token::Type::COLON)) {
                super_constructor_call = this->super_constructor_call();
            }
            consume(Token::Type::LEFT_BRACE, "missing constructor body");
            auto body = block();
            return Constructor {
                    std::move(super_constructor_call),
                    std::make_unique<FunctionDeclaration>(
                        make_span(),
                        init_token,
                        std::move(parameters),
                        std::move(body)
                    ),
                    make_span()
                };
        }
    );
}

ClassObject Parser::class_object() {
    ClassObject object;
    if (match(Token::Type::COLON)) {
        consume(Token::Type::IDENTIFIER, "Expected superclass name");
        object.superclass = current;
    }
    if (match(Token::Type::USING)) {
        do {
            object.traits_used.emplace_back(trait_usage());
        } while (match(Token::Type::COMMA));
    }

    consume(Token::Type::LEFT_BRACE, "missing body");
    while (!check(Token::Type::RIGHT_BRACE) && !check(Token::Type::END)) {
        if (next.string == context_keyword("init")) {
            Token init_token = next;
            auto constructor = this->constructor();
            if (!object.constructor.function) {
                object.constructor = std::move(constructor);
            } else {
                // TODO: better error
                error(init_token, "conflicting constructor", "here");
            }
            continue;
        }

        if (match(Token::Type::OBJECT)) {
            object.metaobject = object_expression();
            continue;
        }

        // TODO: refactor!
        with_source_span(
            [this, &object]-> int {
                auto attributes = member_attributes();
                consume(Token::Type::IDENTIFIER, "missing member name");
                Token member_name = current;

                // Method
                auto span = make_span();
                bool skip_params = attributes[ClassAttributes::GETTER] && !check(Token::Type::LEFT_PAREN);
                if (check(Token::Type::LEFT_PAREN) || skip_params) {
                    if (attributes[ClassAttributes::ABSTRACT]) {
                        object.methods.emplace_back(
                            Method {
                                .attributes = attributes,
                                .function = abstract_method(member_name, skip_params),
                                .span = span
                            }
                        );
                    } else {
                        object.methods.emplace_back(
                            Method {
                                .attributes = attributes,
                                .function = function_declaration_body(member_name, skip_params),
                                .span = span
                            }
                        );
                    }
                    return 0; // TODO
                }

                // Field
                attributes += ClassAttributes::GETTER;
                attributes += ClassAttributes::SETTER;
                object.fields.emplace_back(
                    Field { .attributes = attributes, .variable = var_declaration_body(member_name), .span = span }
                );
                return 0; // TODO
            }
        );
    }

    consume(Token::Type::RIGHT_BRACE, "unmatched }");
    return object;
}

std::unique_ptr<ClassDeclaration> Parser::class_declaration(const bool is_abstract) {
    consume(Token::Type::IDENTIFIER, "missing class name");
    Token class_name = current;
    return std::make_unique<ClassDeclaration>(make_span(), is_abstract, class_name, class_object());
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

Constructor Parser::constructor_statement() {
    Token init_token = current;
    std::vector<Token> parameters = functions_parameters();
    std::optional<SuperConstructorCall> super_constructor;
    // init(parameters*) : super(arguments*) [block]
    if (match(Token::Type::COLON)) {
        with_source_span(
            [this, &super_constructor] -> int {
                consume(Token::Type::SUPER, "missing superclass constructor call");
                super_constructor = super_constructor_call();
                return 0;
            }
        );
    }
    auto span = make_span();
    consume(Token::Type::LEFT_BRACE, "missing constructor body");
    return {
            std::move(super_constructor),
            std::make_unique<FunctionDeclaration>(make_span(), init_token, std::move(parameters), block()),
            span
        };
}

std::unique_ptr<FunctionDeclaration> Parser::abstract_method(const Token& name, const bool skip_params) {
    std::vector<Token> parameters = skip_params ? std::vector<Token>() : functions_parameters();
    consume(Token::Type::SEMICOLON, "missing semicolon after declaration");
    return std::make_unique<FunctionDeclaration>(make_span(), name, std::move(parameters)); // CHECK
}

std::unique_ptr<VariableDeclaration> Parser::abstract_field(const Token& name) {
    consume(Token::Type::SEMICOLON, "missing semicolon after declaration");
    return std::make_unique<VariableDeclaration>(make_span(), name);
}

std::unique_ptr<TraitDeclaration> Parser::trait_declaration() {
    consume(Token::Type::IDENTIFIER, "missing trait name");
    Token trait_name = current;
    std::vector<TraitUsage> traits_used;
    if (match(Token::Type::USING)) {
        do {
            traits_used.push_back(trait_usage());
        } while (match(Token::Type::COMMA));
    }
    consume(Token::Type::LEFT_BRACE, "missing trait body");
    std::vector<Field> fields;
    std::vector<Method> methods;

    while (!check(Token::Type::RIGHT_BRACE) && !check(Token::Type::END)) {
        with_source_span(
            [this, &methods, &fields] -> int {
                auto attributes = member_attributes();
                consume(Token::Type::IDENTIFIER, "invalid trait member.");
                Token member_name = current;

                bool skip_params = attributes[ClassAttributes::GETTER] && !check(Token::Type::LEFT_PAREN);
                auto span = make_span();
                if (check(Token::Type::LEFT_PAREN) || skip_params) {
                    auto fn = in_trait_function(member_name, attributes, skip_params);
                    methods.emplace_back(Method { .attributes = attributes, .function = std::move(fn), .span = span });
                    return 0; // TODO
                }

                // TODO: This probably should not be here
                attributes += ClassAttributes::GETTER;
                attributes += ClassAttributes::SETTER;
                fields.emplace_back(
                    Field { .attributes = attributes, .variable = abstract_field(member_name), .span = span }
                );
                return 0;
            }
        );
    }

    consume(Token::Type::RIGHT_BRACE, "missing '}' after trait body");

    return std::make_unique<TraitDeclaration>(
        make_span(),
        trait_name,
        std::move(methods),
        std::move(fields),
        std::move(traits_used)
    );
}

std::unique_ptr<FunctionDeclaration> Parser::in_trait_function(
    const Token& name,
    bitflags<ClassAttributes>& attributes,
    bool skip_params
) {
    std::vector<Token> parameters = skip_params ? std::vector<Token>() : functions_parameters();
    std::unique_ptr<Expr> body;
    if (match(Token::Type::LEFT_BRACE)) {
        body = block();
    } else {
        // TODO: this probably should not be here but must wait for compiler refactor!
        consume(Token::Type::SEMICOLON, "missing semicolon after declaration");
    }
    return std::make_unique<FunctionDeclaration>(make_span(), name, std::move(parameters), std::move(body));
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

std::unique_ptr<Expr> Parser::expression(const Precedence precedence) {
    return with_source_span(
        [this, precedence] -> std::unique_ptr<Expr> {
            advance();
            std::optional<std::unique_ptr<Expr>> left = prefix();
            if (!left) {
                error(current, "expression expected", "here");
                return std::make_unique<InvalidExpr>(make_span());
            }
            while (precedence < get_precendece(next.type)) {
                advance();
                left = infix(std::move(*left));
            }
            return std::move(*left);
        }
    );
}

std::unique_ptr<Expr> Parser::module_resolution() {
    std::vector path { current };
    advance();
    do {
        consume(Token::Type::IDENTIFIER, "missing module resolution path element");
        path.emplace_back(current);
    } while(match(Token::Type::COLON_COLON));
    return std::make_unique<ModuleResolutionExpr>(make_span(), std::move(path));
}

std::optional<std::unique_ptr<Expr>> Parser::prefix() {
    switch (current.type) {
        case Token::Type::INTEGER: return integer();
        case Token::Type::NUMBER: return number();
        case Token::Type::STRING: return string();
        case Token::Type::TRUE:
        case Token::Type::FALSE:
        case Token::Type::THIS:
        case Token::Type::NIL: return keyword();
        case Token::Type::IDENTIFIER: return check(Token::Type::COLON_COLON) ? module_resolution() : identifier();
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

std::unique_ptr<Expr> Parser::integer() {
    std::string literal = *current.string;
    std::expected<bite_int, ConversionError> result = string_to_int(literal);
    if (!result) {
        error(current, result.error().what());
    }
    return std::make_unique<LiteralExpr>(make_span(), *result);
}

std::unique_ptr<Expr> Parser::number() {
    std::string literal = *current.string;
    std::expected<bite_float, ConversionError> result = string_to_floating(literal);
    if (!result) {
        error(current, result.error().what());
    }
    return std::make_unique<LiteralExpr>(make_span(), *result);
}

std::unique_ptr<Expr> Parser::keyword() {
    switch (current.type) {
        case Token::Type::NIL: return std::make_unique<LiteralExpr>(make_span(), nil_t);
        case Token::Type::FALSE: return std::make_unique<LiteralExpr>(make_span(), false);
        case Token::Type::TRUE: return std::make_unique<LiteralExpr>(make_span(), true);
        case Token::Type::THIS: return std::make_unique<ThisExpr>(make_span());
        default: std::unreachable();
    }
}

std::unique_ptr<Expr> Parser::identifier() {
    return std::make_unique<VariableExpr>(make_span(), current);
}

std::unique_ptr<StringExpr> Parser::string() {
    return std::make_unique<StringExpr>(make_span(), *current.string);
}

std::unique_ptr<Expr> Parser::grouping() {
    auto expr = expression();
    consume(Token::Type::RIGHT_PAREN, "unmatched ')'");
    return expr;
}

std::unique_ptr<UnaryExpr> Parser::unary(const Token::Type operator_type) {
    return std::make_unique<UnaryExpr>(make_span(), expression(Precedence::UNARY), operator_type);
}

std::unique_ptr<SuperExpr> Parser::super_() {
    consume(Token::Type::DOT, "missing '.' after 'super'");
    consume(Token::Type::IDENTIFIER, "missing superclass member identifier after 'super'");
    return std::make_unique<SuperExpr>(make_span(), current);
}

std::unique_ptr<IfExpr> Parser::if_expression() {
    auto condition = expression();
    if (current.type != Token::Type::LEFT_BRACE) {
        error(current, "missing 'if' expression body", "expected '{' here");
    }
    auto then_stmt = block();
    std::unique_ptr<Expr> else_stmt;
    if (match(Token::Type::ELSE)) {
        if (match(Token::Type::IF)) {
            else_stmt = if_expression();
        } else {
            consume(Token::Type::LEFT_BRACE, "missing 'else' expression body.");
            else_stmt = block();
        }
    }
    return std::make_unique<IfExpr>(make_span(), std::move(condition), std::move(then_stmt), std::move(else_stmt));
}

std::unique_ptr<ContinueExpr> Parser::continue_expression() {
    std::optional<Token> label;
    if (match(Token::Type::LABEL)) {
        label = current;
    }

    return std::make_unique<ContinueExpr>(make_span(), label);
}

std::unique_ptr<BreakExpr> Parser::break_expression() {
    std::optional<Token> label;
    if (match(Token::Type::LABEL)) {
        label = current;
    }

    if (!is_expression_start(next.type)) {
        return std::make_unique<BreakExpr>(make_span(), std::unique_ptr<Expr> {}, label);
    }
    return std::make_unique<BreakExpr>(make_span(), expression(), label);
}

std::unique_ptr<ReturnExpr> Parser::return_expression() {
    std::unique_ptr<Expr> expr;
    if (is_expression_start(next.type)) {
        expr = expression();
    }
    return std::make_unique<ReturnExpr>(make_span(), std::move(expr));
}

SuperConstructorCall Parser::super_constructor_call() {
    return with_source_span(
        [this] {
            consume(Token::Type::SUPER, "expected superconsturctor call");
            std::vector<std::unique_ptr<Expr>> superclass_arguments;
            if (match(Token::Type::LEFT_PAREN)) {
                superclass_arguments = call_arguments();
            }
            return SuperConstructorCall { .arguments = std::move(superclass_arguments), .span = make_span() };
        }
    );
}

std::unique_ptr<ObjectExpr> Parser::object_expression() {
    Token name = current;
    return std::make_unique<ObjectExpr>(make_span(), class_object(), name.span);
}

std::unique_ptr<Expr> Parser::labeled_expression() {
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
    error(next, "expression cannot be labeled", "must be either: 'loop', 'for', 'while' or '{'");
    return std::make_unique<InvalidExpr>(make_span());
}

std::unique_ptr<LoopExpr> Parser::loop_expression(const std::optional<Token>& label) {
    consume(Token::Type::LEFT_BRACE, "missing 'loop' expression body");
    return std::make_unique<LoopExpr>(make_span(), block(), label);
}

std::unique_ptr<WhileExpr> Parser::while_expression(const std::optional<Token>& label) {
    auto condition = expression();
    if (current.type != Token::Type::LEFT_BRACE) {
        error(current, "missing 'while' loop body", "expected '{' here");
    }
    auto body = block();
    return std::make_unique<WhileExpr>(make_span(), std::move(condition), std::move(body), label);
}

std::unique_ptr<ForExpr> Parser::for_expression(const std::optional<Token>& label) {
    consume(Token::Type::IDENTIFIER, "invalid 'for' item declaration");
    Token name = current;
    consume(Token::Type::IN, "invalid 'for' loop range expression");
    std::unique_ptr<Expr> iterable = expression();
    if (current.type != Token::Type::LEFT_BRACE) {
        error(current, "invalid 'for' loop body", "expected '{' here");
    }
    auto body = block();
    return std::make_unique<ForExpr>(
        make_span(),
        std::make_unique<VariableDeclaration>(make_span(), name),
        std::move(iterable),
        std::move(body),
        label
    );
}

std::unique_ptr<BlockExpr> Parser::block(const std::optional<Token>& label) {
    // In Bite every block is an expression which can return a value
    // the value that will be returned is the last expression without succeding semicolon.
    // we track that expression here in 'expr_at_end'
    std::vector<std::unique_ptr<Stmt>> stmts;
    std::unique_ptr<Expr> expr_at_end;
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
                stmts.push_back(std::make_unique<ExprStmt>(make_span(), std::move(expr)));
            } else {
                expr_at_end = std::move(expr);
                break;
            }
        }
    }
    consume(Token::Type::RIGHT_BRACE, "unmatched '}'");
    return std::make_unique<BlockExpr>(make_span(), std::move(stmts), std::move(expr_at_end), label);
}

std::unique_ptr<Expr> Parser::infix(std::unique_ptr<Expr> left) {
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


std::unique_ptr<GetPropertyExpr> Parser::dot(std::unique_ptr<Expr> left) {
    consume(Token::Type::IDENTIFIER, "missing property name");
    return std::make_unique<GetPropertyExpr>(make_span(), std::move(left), current);
}

std::unique_ptr<BinaryExpr> Parser::binary(std::unique_ptr<Expr> left) {
    Token::Type op = current.type;
    Precedence precedence = get_precendece(op);
    return std::make_unique<BinaryExpr>(make_span(), std::move(left), expression(precedence), op);
}

std::unique_ptr<BinaryExpr> Parser::assigment(std::unique_ptr<Expr> left) {
    Token::Type op = current.type;
    Precedence precedence = get_precendece(op);
    // lower our precedence to handle right associativity
    // common technique in pratt parsers
    precedence = static_cast<Precedence>(static_cast<int>(precedence) - 1);
    return std::make_unique<BinaryExpr>(make_span(), std::move(left), expression(precedence), op);
}

std::unique_ptr<CallExpr> Parser::call(std::unique_ptr<Expr> left) {
    std::vector<std::unique_ptr<Expr>> arguments = call_arguments();
    return std::make_unique<CallExpr>(make_span(), std::move(left), std::move(arguments));
}
