#include "Parser.h"

#include <cassert>

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
        stmts.push_back(declaration());
    }
    return stmts;
}

Stmt Parser::declaration() {
    auto exit = scope_exit([this] { if (panic_mode) synchronize(); });
    if (match(Token::Type::LET)) {
        return var_declaration();
    }
    if (match(Token::Type::FUN)) {
        return function_declaration();
    }
    return statement();
}

Stmt Parser::statement() {
    if (match(Token::Type::LEFT_BRACE)) {
        return block_statement();
    }
    if (match(Token::Type::IF)) {
        return if_statement();
    }
    if (match(Token::Type::WHILE)) {
        return while_statement();
    }
    if (match(Token::Type::RETURN)) {
        return return_statement();
    }
    return expr_statement();
}

Stmt Parser::var_declaration() {
    consume(Token::Type::IDENTIFIER, "expected identifier");
    Token name = current;
    Expr expr = match(Token::Type::EQUAL) ? expression() : LiteralExpr{nil_t};
    consume(Token::Type::SEMICOLON, "Expected ';' after variable declaration.");
    return VarStmt{.name = name, .value = make_expr_handle(std::move(expr))};
}

Stmt Parser::function_declaration() {
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
    auto body = block_statement();
    return FunctionStmt{
        .name = name,
        .params = std::move(parameters),
        .body = std::make_unique<Stmt>(std::move(body))
    };
}

Stmt Parser::expr_statement() {
    Stmt stmt = ExprStmt{make_expr_handle(expression())};
    consume(Token::Type::SEMICOLON, "Expected ';' after expression");
    return stmt;
}

Stmt Parser::block_statement() {
    std::vector<StmtHandle> stmts;
    while (!check(Token::Type::RIGHT_BRACE) && !check(Token::Type::END)) {
        stmts.push_back(std::make_unique<Stmt>(declaration()));
    }
    consume(Token::Type::RIGHT_BRACE, "Expected '}' after block.");
    return BlockStmt{std::move(stmts)};
}

Stmt Parser::if_statement() {
    consume(Token::Type::LEFT_PAREN, "Expected '(' after 'if'");
    auto condition = expression();
    consume(Token::Type::RIGHT_PAREN, "Expected ')' after 'if' condition");
    auto then_stmt = statement();
    StmtHandle else_stmt = nullptr;
    if (match(Token::Type::ELSE)) {
        else_stmt = std::make_unique<Stmt>(statement());
    }
    return IfStmt{
        .condition = make_expr_handle(std::move(condition)),
        .then_stmt = std::make_unique<Stmt>(std::move(then_stmt)),
        .else_stmt = std::move(else_stmt)
    };
}

Stmt Parser::while_statement() {
    consume(Token::Type::LEFT_PAREN, "Expected '(' after 'while'");
    auto condition = expression();
    consume(Token::Type::RIGHT_PAREN, "Expected ')' after 'while' condition");

    return WhileStmt{
        .condition = make_expr_handle(std::move(condition)),
        .stmt = std::make_unique<Stmt>(declaration())
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
            return Precedence::ASSIGMENT;
        case Token::Type::AND_AND:
            return Precedence::LOGICAL_AND;
        case Token::Type::BAR_BAR:
            return Precedence::LOGICAL_OR;
        case Token::Type::LEFT_PAREN:
            return Precedence::CALL;
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
        case Token::Type::BANG:
        case Token::Type::MINUS:
        case Token::Type::TILDE:
            return unary(current.type);
        default: return {};
    }
}

Expr Parser::integer() const {
    // todo temp
    return LiteralExpr {std::stoll(current.get_lexeme(lexer.get_source()))};
}

Expr Parser::number() const {
    // todo better number parsing
    return LiteralExpr{std::stod(current.get_lexeme(lexer.get_source()))};
}

Expr Parser::string() const {
    return StringLiteral{std::string(current.get_lexeme(lexer.get_source()))};
}

Expr Parser::identifier() {
    Token name = current;
    if (match(Token::Type::EQUAL)) {
        return AssigmentExpr{.identifier = name, .expr = make_expr_handle(expression())};
    }
    return VariableExpr{name};
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
        case Token::Type::LEFT_PAREN:
            return call(std::move(left));
        default:
            return left;
    }
}

Expr Parser::binary(Expr left) {
    Token::Type op = current.type;
    return BinaryExpr{
        make_expr_handle(std::move(left)),
        make_expr_handle(expression(get_precendece(op))), op
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
