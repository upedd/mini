#include "Parser.h"


Token Parser::advance() {
    current = next;
    while (true) { // todo is this loop safe?
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

void Parser::consume(Token::Type type, std::string_view message) {
    if (next.type == type) {
        advance();
        return;
    }
    error(current, message);
}

Expr Parser::call(Expr left) {
    std::vector<ExprHandle> arguments;
    if(!check(Token::Type::RIGHT_PAREN)) {
        do {
            arguments.push_back(std::make_unique<Expr>(expression()));
        } while (match(Token::Type::COMMA));
    }
    consume(Token::Type::RIGHT_PAREN, "Expected ')' after call arguments.");
    return CallExpr {.callee = std::make_unique<Expr>(std::move(left)), .arguments = std::move(arguments)};
}

Expr Parser::grouping() {
    auto expr = expression();
    consume(Token::Type::RIGHT_PAREN, "Expected ')' after expression.");
    return expr;
}

Expr Parser::integer() const {
    // todo temp
    return LiteralExpr {std::stoll(current.get_lexeme(lexer.get_source()))};
}

Expr Parser::unary(Token::Type op) {
    return UnaryExpr { std::make_unique<Expr>(expression(Precedence::UNARY)), op };
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
            return literal();
        case Token::Type::BANG:
        case Token::Type::MINUS:
        case Token::Type::TILDE:
            return unary(current.type);
        case Token::Type::LEFT_PAREN:
            return grouping();
        case Token::Type::IDENTIFIER:
            return identifier();
        default: return {};
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
        case Token::Type::PERCENT:
            return binary(std::move(left));
        case Token::Type::LEFT_PAREN:
            return call(std::move(left));
        default:
            return left;
    }
}

Expr Parser::number() {
    // todo better number parsing
    return LiteralExpr {std::stod(current.get_lexeme(lexer.get_source()))};
}

Expr Parser::literal() {
    // handle error
    switch (current.type) {
        case Token::Type::NIL:
            return LiteralExpr {nil_t};
        case Token::Type::FALSE:
            return LiteralExpr {false};
        case Token::Type::TRUE:
            return LiteralExpr {true};
    }
}

Expr Parser::string() {
    return StringLiteral {std::string(current.get_lexeme(lexer.get_source()))};
}

Expr Parser::identifier() {
    Token name = current;
    if (match(Token::Type::EQUAL)) {
        return AssigmentExpr {.identifier = name, .expr = std::make_unique<Expr>(expression())};
    }
    return VariableExpr {name};
}


Expr Parser::binary(Expr left) {
    Token::Type op = current.type;
    return BinaryExpr {std::make_unique<Expr>(std::move(left)),
        std::make_unique<Expr>(expression(get_precendece(op))), op};
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

bool Parser::match(Token::Type type) {
    if (next.type == type) {
        advance();
        return true;
    }
    return false;
}

Stmt Parser::var_declaration() {
    consume(Token::Type::IDENTIFIER, "expected identifier");
    Token name = current;
    Expr expr = match(Token::Type::EQUAL) ? expression() : LiteralExpr {nil_t};
    consume(Token::Type::SEMICOLON, "Expected ';' after variable declaration.");
    return VarStmt {.name = name, .value = std::make_unique<Expr>(std::move(expr))};
}

Stmt Parser::expr_statement() {
    Stmt stmt = ExprStmt {std::make_unique<Expr>(expression())};
    consume(Token::Type::SEMICOLON, "Expected ';' after expression");
    return stmt;
}

bool Parser::check(Token::Type type) {
    return next.type == type;
}

Stmt Parser::block() {
    std::vector<StmtHandle> stmts;
    while (!check(Token::Type::RIGHT_BRACE) && !check(Token::Type::END)) {
        stmts.push_back(std::make_unique<Stmt>(declaration()));
    }
    consume(Token::Type::RIGHT_BRACE, "Expected '}' after block.");
    return BlockStmt {std::move(stmts)};
}

Stmt Parser::if_statement() {
    consume(Token::Type::LEFT_PAREN, "Expected '(' after 'if'");
    auto condition = expression();
    consume(Token::Type::RIGHT_PAREN, "Expected ')' after 'if' condition");
    auto then_stmt = declaration();
    StmtHandle else_stmt = nullptr;
    if (match(Token::Type::ELSE)) {
        else_stmt = std::make_unique<Stmt>(declaration());
    }
    return IfStmt {.condition = std::make_unique<Expr>(std::move(condition)), .then_stmt = std::make_unique<Stmt>(std::move(then_stmt)), .else_stmt = std::move(else_stmt)};
}

Stmt Parser::while_statement() {
    consume(Token::Type::LEFT_PAREN, "Expected '(' after 'while'");
    auto condition = expression();
    consume(Token::Type::RIGHT_PAREN, "Expected ')' after 'while' condition");

    return WhileStmt {.condition = std::make_unique<Expr>(std::move(condition)), .stmt = std::make_unique<Stmt>(declaration())};
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
    auto body = block();
    return FunctionStmt {.name = name, .params = std::move(parameters), .body = std::make_unique<Stmt>(std::move(body))};
}

Stmt Parser::declaration() {
    if (match(Token::Type::LET)) {
        return var_declaration();
    }
    if (match(Token::Type::LEFT_BRACE)) {
        return block();
    }
    if (match(Token::Type::IF)) {
        return if_statement();
    }
    if (match(Token::Type::WHILE)) {
        return while_statement();
    }
    if (match(Token::Type::FUN)) {
        return function_declaration();
    }
    return expr_statement();
}

std::vector<Stmt> Parser::parse() {
    advance(); // populate current
    std::vector<Stmt> stmts;
    while (!match(Token::Type::END)) {
        stmts.push_back(declaration());
    }
    return stmts;
}

Expr Parser::expression(Precedence precedence) {
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

void Parser::error(const Token &token, std::string_view message) {
    if (panic_mode) return;
    panic_mode = true;
    errors.emplace_back(token, message);
}

const std::vector<Parser::Error> & Parser::get_errors() {
    return errors;
}



