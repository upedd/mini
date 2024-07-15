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
            std::cout << token->to_string(lexer.get_source()) << '\n';
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
    } else {
        auto expr = std::make_unique<Expr>(expression());
        consume(Token::Type::SEMICOLON, "Expected ';' after expression.");
        return ExprStmt(std::move(expr));
    }
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
    return ForExpr {.name = name,
        .iterable = make_expr_handle(std::move(iterable)),
        .body = make_expr_handle(std::move(body)),
        .label = label};
}

std::optional<Stmt> Parser::statement() {
    auto exit = scope_exit([this] { if (panic_mode) synchronize(); });
    if (match(Token::Type::RETURN)) {
        return return_statement();
    }
    if (match(Token::Type::LET)) {
        return var_declaration();
    }
    if (match(Token::Type::FUN)) {
        return function_declaration();
    }
    if (match(Token::Type::CLASS)) {
        return class_declaration();
    }
    if (match(Token::Type::NATIVE)) {
        return native_declaration();
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
    return {};
}

Stmt Parser::var_declaration() {
    consume(Token::Type::IDENTIFIER, "expected identifier");
    Token name = current;
    Expr expr = match(Token::Type::EQUAL) ? expression() : LiteralExpr{nil_t};
    consume(Token::Type::SEMICOLON, "Expected ';' after variable declaration.");
    return VarStmt{.name = name, .value = make_expr_handle(std::move(expr))};
}

FunctionStmt Parser::function_declaration() {
    consume(Token::Type::IDENTIFIER, "expected function name");
    Token name = current;
    consume(Token::Type::LEFT_PAREN, "Expected '(' after function name");
    std::vector<Token> parameters;
    if (!check(Token::Type::RIGHT_PAREN)) {
        do {
            consume(Token::Type::IDENTIFIER, "Expected identifier");
            parameters.push_back(current);
        } while (match(Token::Type::COMMA));
    }
    consume(Token::Type::RIGHT_PAREN, "Expected ')' after function parameters");
    consume(Token::Type::LEFT_BRACE, "Expected '{' before function body");
    auto body = block();
    return FunctionStmt{
        .name = name,
        .params = std::move(parameters),
        .body = std::make_unique<Expr>(std::move(body))
    };
}

Stmt Parser::class_declaration() {
    consume(Token::Type::IDENTIFIER, "Expected class name.");
    Token name = current;


    std::optional<Token> super_name {};
    if (match(Token::Type::LESS)) {
        consume(Token::Type::IDENTIFIER, "Expected superclass name.");
        super_name = current;
    }
    consume(Token::Type::LEFT_BRACE, "Expected '{' before class body.");

    std::vector<std::unique_ptr<FunctionStmt>> methods;
    // todo: fix!
    while (!check(Token::Type::RIGHT_BRACE) && !check(Token::Type::END)) {
        methods.push_back(std::make_unique<FunctionStmt>(function_declaration()));
    }
    consume(Token::Type::RIGHT_BRACE, "Expected '}' after class body.");

    return ClassStmt {.name = name, .methods = std::move(methods), .super_name = super_name};
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

Stmt Parser::return_statement() {
    ExprHandle expr = nullptr;
    if (!match(Token::Type::SEMICOLON)) {
        expr = make_expr_handle(expression());
        consume(Token::Type::SEMICOLON, "Exepected ';' after return value.");
    }
    return ReturnStmt{std::move(expr)};
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
    return VariableExpr{current};
}

Expr Parser::super_() {
    consume(Token::Type::DOT, "Expected '.' after 'super'.");
    consume(Token::Type::IDENTIFIER, "Expected superclass method name.");
    return SuperExpr{current};
}

Expr Parser::block(std::optional<Token> label) {
    std::vector<StmtHandle> stmts;
    ExprHandle expr_at_end = nullptr;
    while (!check(Token::Type::RIGHT_BRACE) && !check(Token::Type::END)) {
        if (auto stmt = statement()) {
            stmts.push_back(std::make_unique<Stmt>(std::move(*stmt)));
        } else {
            auto expr = expression();
            if (match(Token::Type::SEMICOLON)) {
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
        return true;
        default: return false;
    }
}

Expr Parser::break_expression() {
    std::optional<Token> label;
    if (match(Token::Type::LABEL)) {
        label = current;
    }
    if (!has_prefix(next.type)) {
        return BreakExpr{};
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
    consume(Token::Type::COLON, "Expected ':' after label");
    // all expressions we can break out of
    if (match(Token::Type::LOOP)) {
        return loop_expression(current);
    }
    if (match(Token::Type::WHILE)) {
        return while_expression(current);
    }
    if (match(Token::Type::FOR)) {
        return for_expression(current);
    }
    if (match(Token::Type::LEFT_BRACE)) {
        return block(current);
    }
    error(current, "Only loops and blocks can be labeled.");
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
        default: return {};
    }
}

Expr Parser::integer() {
    std::string literal = current.get_lexeme(lexer.get_source());
    std::expected<bite_int, ConversionError> result = string_to_int(literal);
    if (!result) {
        error(current, result.error().what());
    }
    return LiteralExpr {*result};
}

Expr Parser::number() {
    std::string literal = current.get_lexeme(lexer.get_source());
    std::expected<bite_float, ConversionError> result = string_to_floating(literal);
    if (!result) {
        error(current, result.error().what());
    }
    return LiteralExpr {*result};
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
    return GetPropertyExpr {.left = make_expr_handle(std::move(left)), .property = current};
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
    if (expect_lvalue && !std::holds_alternative<VariableExpr>(left) && !std::holds_alternative<GetPropertyExpr>(left)) {
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
