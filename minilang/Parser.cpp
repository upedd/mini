//
// Created by MiłoszK on 19.06.2024.
//

#include "Parser.h"

#include "Mini.h"

std::vector<std::unique_ptr<Stmt>> Parser::parse() {
    std::vector<std::unique_ptr<Stmt>> statements;
    while (!is_at_end()) {
        statements.push_back(declaration());
    }
    return statements;
}

Parser::ParseError Parser::error(const Token &token, std::string_view message) {
    Mini::error(token, message);
    return {};
}

Token Parser::consume(TokenType type, std::string_view message) {
    if (check(type)) return advance();

    throw error(peek(), message);
}

void Parser::synchronize() {
    advance();
    while (!is_at_end()) {
        if (previous().type == TokenType::SEMICOLON) return;
        switch (peek().type) {
            case TokenType::CLASS:
            case TokenType::FUN:
            case TokenType::VAR:
            case TokenType::FOR:
            case TokenType::IF:
            case TokenType::WHILE:
            case TokenType::PRINT:
            case TokenType::RETURN:
                return;
            default:
                break;
        }

        advance();
    }
}

std::unique_ptr<Stmt> Parser::printStatement() {
    auto value = expression();
    consume(TokenType::SEMICOLON, "Expect ';' after value.");
    return std::make_unique<Stmt::Print>(std::move(value));
}

std::unique_ptr<Stmt> Parser::expressionStatement() {
    auto expr = expression();
    consume(TokenType::SEMICOLON, "Expect ';' after expression.");
    return std::make_unique<Stmt::Expression>(std::move(expr));
}

std::vector<std::unique_ptr<Stmt>> Parser::block() {
    std::vector<std::unique_ptr<Stmt>> statements;
    while (!check(TokenType::RIGHT_BRACE) && !is_at_end()) {
        statements.push_back(declaration());
    }
    consume(TokenType::RIGHT_BRACE, "Expect '}' after block.");
    return statements;
}

std::unique_ptr<Stmt> Parser::if_statement() {
    consume(TokenType::LEFT_PAREN, "Expect '(' after 'if'.");
    auto condition = expression();
    consume(TokenType::RIGHT_PAREN, "Expect ')' after if condition.");
    auto then_branch = statement();
    std::unique_ptr<Stmt> else_branch;
    if (match({TokenType::ELSE})) {
        else_branch = statement();
    }

    return std::make_unique<Stmt::If>(std::move(condition), std::move(then_branch), std::move(else_branch));
}

std::unique_ptr<Stmt> Parser::while_statement() {
    consume(TokenType::LEFT_PAREN, "Expect '(' after 'while'.");
    auto condition = expression();
    consume(TokenType::RIGHT_PAREN, "Expect ')' after while condition.");
    auto body = statement();
    return std::make_unique<Stmt::While>(std::move(condition), std::move(body));
}

std::unique_ptr<Stmt> Parser::for_statement() {
    consume(TokenType::LEFT_PAREN, "Expect '(' after 'for'.");
    std::unique_ptr<Stmt> initializer;
    if (match({TokenType::SEMICOLON})) {
        initializer = nullptr;
    } else if (match({TokenType::VAR})) {
        initializer = var_declaration();
    } else {
        initializer = expressionStatement();
    }

    std::unique_ptr<Expr> condition;
    if (!check(TokenType::SEMICOLON)) {
        condition = expression();
    }
    consume(TokenType::SEMICOLON, "Expect ';' after loop condition");
    std::unique_ptr<Expr> increment;
    if (!check(TokenType::RIGHT_PAREN)) {
        increment = expression();
    }
    consume(TokenType::RIGHT_PAREN, "Expect '(' after for clauses.");

    auto body = statement();

    if (increment) {
        auto stmt_expr = std::make_unique<Stmt::Expression>(std::move(increment));
        // TODO: workaround???????
        std::vector<std::unique_ptr<Stmt>> statements(2);
        statements[0] = std::move(body);
        statements[1] = std::move(stmt_expr);
        body = std::make_unique<Stmt::Block>(std::move(statements));
        // statements.emplace_back(std::move(body));
        // statements.emplace_back(std::move(stmt_expr));
        // body = std::make_unique<Stmt::Block>(statements);
    }

    if (!condition) {
        condition = std::make_unique<Expr::Literal>(true);
    }
    body = std::make_unique<Stmt::While>(std::move(condition), std::move(body));

    if (initializer) {
        std::vector<std::unique_ptr<Stmt>> statements(2);
        statements[0] = std::move(initializer);
        statements[1] = std::move(body);
        body = std::make_unique<Stmt::Block>(std::move(statements));
    }
    return body;
}

std::unique_ptr<Stmt> Parser::statement() {
    if (match({TokenType::PRINT})) return printStatement();
    if (match({TokenType::LEFT_BRACE})) return std::make_unique<Stmt::Block>(block());
    if (match({TokenType::IF})) return if_statement();
    if (match({TokenType::WHILE})) return while_statement();
    if (match({TokenType::FOR})) return for_statement();
    return expressionStatement();
}

std::unique_ptr<Stmt> Parser::var_declaration() {
    Token name = consume(TokenType::IDENTIFIER, "Expect variable name.");

    std::unique_ptr<Expr> initializer = std::make_unique<Expr::Literal>(nullptr);
    if (match({TokenType::EQUAL})) {
        initializer = expression();
    }
    consume(TokenType::SEMICOLON, "Expect ';' after variable declaration.");
    return std::make_unique<Stmt::Var>(name, std::move(initializer));
}

std::unique_ptr<Stmt> Parser::function(const std::string &kind) {
    Token name = consume(TokenType::IDENTIFIER, "Expect " + kind + " name.");
    consume(TokenType::LEFT_PAREN, "Expect '(' after " + kind  + " name.");
    std::vector<Token> parameters;
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            if (parameters.size() >= 255) {
                error(peek(), "Can't have more than 255 parameters.");
            }

            parameters.push_back(consume(TokenType::IDENTIFIER, "Expect parameter name."));
        } while (match({TokenType::COMMA}));
    }
    consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TokenType::LEFT_BRACE, "Expect '{' before " + kind + " body.");
    auto body = block();
    return std::make_unique<Stmt::Function>(name, std::move(parameters), std::move(body));
}

std::unique_ptr<Stmt> Parser::declaration() {
    try {
        if (match({TokenType::FUN})) return function("function");
        if (match({TokenType::VAR})) return var_declaration();
        return statement();
    } catch (const ParseError& error) {
        synchronize();
        return nullptr;
    }
}

std::unique_ptr<Expr> Parser::primary() {
    if (match({TokenType::FALSE})) return std::make_unique<Expr::Literal>(false);
    if (match({TokenType::TRUE})) return std::make_unique<Expr::Literal>(true);
    if (match({TokenType::NIL})) return std::make_unique<Expr::Literal>(nullptr); // can we do that?

    if (match({TokenType::NUMBER, TokenType::STRING})) {
        return std::make_unique<Expr::Literal>(previous().literal);
    }

    if (match({TokenType::IDENTIFIER})) {
        return std::make_unique<Expr::Variable>(previous());
    }

    if (match({TokenType::LEFT_PAREN})) {
        auto expr = expression();
        consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
        return std::make_unique<Expr::Grouping>(std::move(expr));
    }

    throw error(peek(), "Expect expression.");
}

std::unique_ptr<Expr> Parser::finish_call(std::unique_ptr<Expr> &&callee) {
    std::vector<std::shared_ptr<Expr>> arguments;
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            if (arguments.size() >= 255) {
                error(peek(), "Can't have more than 255 arguments.");
            }
            arguments.push_back(expression());
        } while (match({TokenType::COMMA}));
    }

    Token paren = consume(TokenType::RIGHT_PAREN, "EXpect ')' after arguments.");

    return std::make_unique<Expr::Call>(std::move(callee), paren, arguments);
}

std::unique_ptr<Expr> Parser::call() {
    auto expr = primary();
    while (true) {
        if (match({TokenType::LEFT_PAREN})) {
            expr = finish_call(std::move(expr));
        } else break;
    }
    return expr;
}

std::unique_ptr<Expr> Parser::unary() {
    if (match({TokenType::BANG, TokenType::MINUS})) {
        Token op = previous();
        auto right = unary();
        return std::make_unique<Expr::Unary>(op, std::move(right));
    }

    return call();
}

std::unique_ptr<Expr> Parser::factor() {
    auto expr = unary();

    while (match({TokenType::SLASH, TokenType::STAR})) {
        Token op = previous();
        auto right = unary();
        expr = std::make_unique<Expr::Binary>(std::move(expr), op, std::move(right));
    }

    return expr;
}

std::unique_ptr<Expr> Parser::term() {
    auto expr = factor();

    while (match({TokenType::PLUS, TokenType::MINUS})) {
        Token op = previous();
        auto right = factor();
        expr = std::make_unique<Expr::Binary>(std::move(expr), op, std::move(right));
    }

    return expr;
}

std::unique_ptr<Expr> Parser::comparison() {
    auto expr = term();

    while (match({TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::LESS, TokenType::LESS})) {
        Token op = previous();
        auto right = term();
        expr = std::make_unique<Expr::Binary>(std::move(expr), op, std::move(right));
    }

    return expr;
}

std::unique_ptr<Expr> Parser::equality() {
    auto expr = comparison();
    while (match({TokenType::BANG_EQUAL, TokenType::EQUAL_EQUAL})) {
        Token op = previous();
        auto right = comparison();
        expr = std::make_unique<Expr::Binary>(std::move(expr), op, std::move(right));
    }

    return expr;
}

std::unique_ptr<Expr> Parser::_and() {
    auto expr = equality();

    while (match({TokenType::AND})) {
        Token op = previous();
        auto right = equality();
        expr = std::make_unique<Expr::Logical>(std::move(expr), op, std::move(right));
    }

    return expr;
}

std::unique_ptr<Expr> Parser::_or() {
    auto expr = _and();
    while (match({TokenType::OR})) {
        Token op = previous();
        auto right = _and();
        expr = std::make_unique<Expr::Logical>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::assigment() {
    auto expr = _or();

    if (match({TokenType::EQUAL})) {
        Token equals = previous();
        auto value = assigment();
        if (auto* var = dynamic_cast<Expr::Variable*>(expr.get())) {
            Token name = var->name;
            return std::make_unique<Expr::Assign>(name, std::move(value));
        }

        error(equals, "Invalid assigment target.");
    }

    return expr;
}

std::unique_ptr<Expr> Parser::expression() {
    return assigment();
}

bool Parser::check(TokenType type) {
    if (is_at_end()) return false;
    return peek().type == type;
}

Token Parser::peek() {
    return tokens[current];
}

bool Parser::is_at_end() {
    return peek().type == TokenType::END;
}

Token Parser::previous() {
    return tokens[current - 1];
}

Token Parser::advance() {
    if (!is_at_end()) current++;
    return previous();
}

bool Parser::match(const std::vector<TokenType> &types) {
    for (auto type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}
