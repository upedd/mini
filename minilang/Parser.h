//
// Created by MiłoszK on 19.06.2024.
//

#ifndef PARSER_H
#define PARSER_H
#include <vector>
#include <memory>

#include "Token.h"
#include "generated/Expr.h"
#include "generated/Stmt.h"


class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {}


    

    std::vector<std::unique_ptr<Stmt>> parse();

private:
    class ParseError : public std::runtime_error {
    public:
        ParseError() : runtime_error("parse error") {}
    };

    static ParseError error(const Token & token, std::string_view message);
    Token consume(TokenType type, std::string_view message);
    void synchronize();

    std::unique_ptr<Stmt> printStatement();
    std::unique_ptr<Stmt> expressionStatement();

    std::vector<std::unique_ptr<Stmt>> block();

    std::unique_ptr<Stmt> if_statement();

    std::unique_ptr<Stmt> while_statement();

    std::unique_ptr<Stmt> for_statement();

    std::unique_ptr<Stmt> statement();

    std::unique_ptr<Stmt> var_declaration();

    std::unique_ptr<Stmt> declaration();

    std::unique_ptr<Expr> primary();

    std::unique_ptr<Expr> finish_call(std::unique_ptr<Expr> &&callee);

    std::unique_ptr<Expr> call();

    std::unique_ptr<Expr> unary();
    std::unique_ptr<Expr> factor();
    std::unique_ptr<Expr> term();
    std::unique_ptr<Expr> comparison();
    std::unique_ptr<Expr> equality();

    std::unique_ptr<Expr> _and();

    std::unique_ptr<Expr> _or();

    std::unique_ptr<Expr> assigment();

    std::unique_ptr<Expr> expression();

    bool check(TokenType type);

    Token peek();

    bool is_at_end();

    Token previous();

    Token advance();
    bool match(const std::vector<TokenType>& types);

    std::vector<Token> tokens;
    int current = 0;
};



#endif //PARSER_H
