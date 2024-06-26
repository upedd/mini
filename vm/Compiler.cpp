//
// Created by MiłoszK on 23.06.2024.
//

#include "Compiler.h"

#include <iostream>

#include "debug.h"
#include "Scanner.h"
using namespace vm;

Compiler::ParseRule* Compiler::get_rule(Token::Type type) {
    return &rules[static_cast<int>(type)];
}

void Compiler::error(std::string_view message) {
    error_at(previous, message);
}

void Compiler::error_at(const Token &token, std::string_view message) {
    if (panic_mode) return;
    panic_mode = true;
    std::cerr << "[line " << token.line << "] Error";
    if (token.type == Token::Type::END) {
        std::cerr << " at end";
    } else if (token.type != Token::Type::ERROR) {
        std::cerr << " at '" << token.lexeme << '\'';
    }
    std::cerr << ": " << message;
    had_error = true;
}

void Compiler::error_at_current(std::string_view message) {
    error_at(current, message);
}

void Compiler::advance() {
    previous = current;
    while (true) {
        current = scanner.scan_token();
        if (current.type != Token::Type::ERROR) break;
        error_at_current(current.lexeme);
    }
}

void Compiler::consume(Token::Type type, std::string_view message) {
    if (current.type == type) {
        advance();
        return;
    }
    error_at_current(message);
}

void Compiler::expression() {
    parse_precendence(Precedence::ASSIGMENT);
}

void Compiler::grouping(bool can_assign) {
    expression();
    consume(Token::Type::RIGHT_PAREN, "Expect ')' after expression.");
}

void Compiler::unary(bool can_assign) {
    Token::Type operator_type = previous.type;
    parse_precendence(Precedence::UNARY);
    switch (operator_type) {
        case Token::Type::MINUS: emit_byte(static_cast<uint8_t>(Instruction::OpCode::NEGATE)); break;
        case Token::Type::BANG: emit_byte(static_cast<uint8_t>(Instruction::OpCode::NOT)); break;
        default: return; // unreachable
    }
}

void Compiler::binary(bool can_assign) {
    Token::Type operator_type = previous.type;
    ParseRule* rule = get_rule(operator_type);
    parse_precendence(static_cast<Precedence>(static_cast<int>(rule->precedence) + 1));

    switch (operator_type) {
        case Token::Type::PLUS: emit_byte(static_cast<int>(Instruction::OpCode::ADD)); break;
        case Token::Type::MINUS: emit_byte(static_cast<int>(Instruction::OpCode::SUBTRACT)); break;
        case Token::Type::STAR: emit_byte(static_cast<int>(Instruction::OpCode::MULTIPLY)); break;
        case Token::Type::SLASH: emit_byte(static_cast<int>(Instruction::OpCode::DIVIDE)); break;
        case Token::Type::BANG_EQUAL: emit_bytes({static_cast<uint8_t>(Instruction::OpCode::EQUAL), static_cast<uint8_t>(Instruction::OpCode::NOT)});
        case Token::Type::EQUAL_EQUAL: emit_byte(static_cast<int>(Instruction::OpCode::EQUAL)); break;
        case Token::Type::GREATER: emit_byte(static_cast<int>(Instruction::OpCode::GREATER)); break;
        case Token::Type::GREATER_EQUAL: emit_bytes({static_cast<uint8_t>(Instruction::OpCode::LESS), static_cast<uint8_t>(Instruction::OpCode::NOT)});
        case Token::Type::LESS: emit_byte(static_cast<int>(Instruction::OpCode::LESS)); break;
        case Token::Type::LESS_EQUAL: emit_bytes({static_cast<uint8_t>(Instruction::OpCode::GREATER), static_cast<uint8_t>(Instruction::OpCode::NOT)});
        default: return;
    }
}

void Compiler::parse_precendence(Precedence precendence) {
    advance();
    ParseFn prefix_rule = get_rule(previous.type)->prefix;
    if (prefix_rule == nullptr) {
        error("Expect expression.");
        return;
    }

    bool can_assign = precendence <= Precedence::ASSIGMENT;

    (this->*prefix_rule)(can_assign);

    while (precendence <= get_rule(current.type)->precedence) {
        advance();
        ParseFn infix_rule = get_rule(previous.type)->infix;
        (this->*infix_rule)(can_assign);
    }

    if (can_assign && match(Token::Type::EQUAL)) {
        error("Invalid assigment target.");
    }
}

uint8_t Compiler::make_constant(Value value) {
    int constant = chunk.add_constant(value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }
    return constant;
}

void Compiler::emit_constant(Value value) {
    emit_bytes({static_cast<uint8_t>(Instruction::OpCode::CONSTANT), make_constant(value)});
}

void Compiler::number(bool can_assign) {
    double value = std::stod(previous.lexeme.data());
    emit_constant(Value::make_number(value));
}

void Compiler::string(bool can_assign) {
    // TODO should we dealoc literals?
    emit_constant(
        Value::make_object(
            vm.allocate_string(
                std::string(previous.lexeme.substr(1, previous.lexeme.size() - 2))
                )
            )
        );
}

void Compiler::literal(bool can_assign) {
    switch (previous.type) {
        case Token::Type::FALSE: emit_byte(static_cast<uint8_t>(Instruction::OpCode::FALSE)); break;
        case Token::Type::NIL: emit_byte(static_cast<uint8_t>(Instruction::OpCode::NIL)); break;
        case Token::Type::TRUE: emit_byte(static_cast<uint8_t>(Instruction::OpCode::TRUE)); break;
        default: return;
    }
}

void Compiler::_and(bool can_assign) {
    int end_jump = emit_jump(Instruction::OpCode::JUMP_IF_FALSE);
    emit_byte(static_cast<uint8_t>(Instruction::OpCode::POP));
    parse_precendence(Precedence::AND);
    patch_jump(end_jump);
}

void Compiler::_or(bool can_assign) {
    int else_jump = emit_jump(Instruction::OpCode::JUMP_IF_FALSE);
    int end_jump = emit_jump(Instruction::OpCode::JUMP);
    patch_jump(else_jump);
    emit_byte(static_cast<uint8_t>(Instruction::OpCode::POP));
    parse_precendence(Precedence::OR);
    patch_jump(end_jump);
}

int Compiler::resolve_local(const Token &name) {
    for (int i = local_count - 1; i >= 0; --i) {
        if (locals[i].name.lexeme == name.lexeme) {
            if (locals[i].depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

void Compiler::named_variable(const Token &name, bool can_assign) {
    uint8_t get_op, set_op;
    int arg = resolve_local(name);
    if (arg != -1) {
        get_op = static_cast<uint8_t>(Instruction::OpCode::GET_LOCAL);
        set_op = static_cast<uint8_t>(Instruction::OpCode::SET_LOCAL);
    } else {
        arg = identifier_constant(name);
        get_op = static_cast<uint8_t>(Instruction::OpCode::GET_GLOBAL);
        set_op = static_cast<uint8_t>(Instruction::OpCode::SET_GLOBAL);
    }
    if (can_assign && match(Token::Type::EQUAL)) {
        expression();
        emit_bytes({set_op, static_cast<uint8_t>(arg)});
    } else {
        emit_bytes({get_op, static_cast<uint8_t>(arg)});
    }


}

void Compiler::variable(bool can_assign) {
    return named_variable(previous, can_assign);
}

bool Compiler::check(Token::Type type) {
    return current.type == type;
}

bool Compiler::match(Token::Type type) {
    if (!check(type)) return false;
    advance();
    return true;
}

void Compiler::print_statement() {
    expression();
    consume(Token::Type::SEMICOLON, "Expect ';' after value.");
    emit_byte(static_cast<uint8_t>(Instruction::OpCode::PRINT));
}

void Compiler::expression_statement() {
    expression();
    consume(Token::Type::SEMICOLON, "Expect ';' after expression.");
    emit_byte(static_cast<uint8_t>(Instruction::OpCode::POP));
}

void Compiler::block() {
    while (!check(Token::Type::RIGHT_BRACE) && !check(Token::Type::END)) {
        declaration();
    }
    consume(Token::Type::RIGHT_BRACE, "Expect '}' after block.");
}

void Compiler::begin_scope() {
    scope_depth++;
}

void Compiler::end_scope() {
    scope_depth--;

    while (local_count > 0 && locals[local_count - 1].depth > scope_depth) {
        emit_byte(static_cast<uint8_t>(Instruction::OpCode::POP));
        local_count--;
    }
}

int Compiler::emit_jump(Instruction::OpCode op_code) {
    emit_byte(static_cast<uint8_t>(op_code));
    emit_byte(0xFF);
    emit_byte(0xFF);
    return chunk.get_code().size() - 2;
}

void Compiler::patch_jump(int offset) {
    int jump = chunk.get_code().size() - offset - 2;
    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    chunk.get_code()[offset] = (jump >> 8) & 0xFF;
    chunk.get_code()[offset + 1] = jump & 0xFF;
}

void Compiler::if_statement() {
    consume(Token::Type::LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(Token::Type::RIGHT_PAREN, "Expect ')' after condition.");
    int then_jump = emit_jump(Instruction::OpCode::JUMP_IF_FALSE);
    emit_byte(static_cast<uint8_t>(Instruction::OpCode::POP));
    statement();

    int else_jump = emit_jump(Instruction::OpCode::JUMP);

    patch_jump(then_jump);
    emit_byte(static_cast<uint8_t>(Instruction::OpCode::POP));
    if (match(Token::Type::ELSE)) statement();
    patch_jump(else_jump);
}

void Compiler::emit_loop(int loop_start) {
    emit_byte(static_cast<uint8_t>(Instruction::OpCode::LOOP));
    int offset = chunk.get_code().size() - loop_start + 2;
    if (offset > UINT16_MAX) {
        error("Loop body too large");
    }

    emit_byte((offset >> 8) & 0xFF);
    emit_byte(offset & 0xFF);
}

void Compiler::while_statement() {
    int loop_start = chunk.get_code().size();
    consume(Token::Type::LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(Token::Type::RIGHT_PAREN, "Expect ')' after condition.");
    int exit_jump = emit_jump(Instruction::OpCode::JUMP_IF_FALSE);
    emit_byte(static_cast<uint8_t>(Instruction::OpCode::POP));
    statement();
    emit_loop(loop_start);
    patch_jump(exit_jump);
    emit_byte(static_cast<uint8_t>(Instruction::OpCode::POP));
}

void Compiler::for_statement() {
    begin_scope();
    consume(Token::Type::LEFT_PAREN, "Expect '(' after for.");
    if (match(Token::Type::VAR)) {
        var_declaration();
    } else if (!match(Token::Type::SEMICOLON)) {
        expression_statement();
    }

    int loop_start = chunk.get_code().size();
    int exit_jump = -1;
    if (!match(Token::Type::SEMICOLON)) {
        expression();
        consume(Token::Type::SEMICOLON, "Expect ';' after loop condition.");
        exit_jump = emit_jump(Instruction::OpCode::JUMP_IF_FALSE);
        emit_byte(static_cast<uint8_t>(Instruction::OpCode::POP));
    }

    if (!match(Token::Type::RIGHT_PAREN)) {
        int body_jump = emit_jump(Instruction::OpCode::JUMP);
        int inc_start = chunk.get_code().size();
        expression();
        emit_byte(static_cast<uint8_t>(Instruction::OpCode::POP));
        consume(Token::Type::RIGHT_PAREN, "Expect ')' after for clauses.");

        emit_loop(loop_start);
        loop_start = inc_start;
        patch_jump(body_jump);
    }





    statement();

    emit_loop(loop_start);

    if (exit_jump != -1) {
        patch_jump(exit_jump);
        emit_byte(static_cast<uint8_t>(Instruction::OpCode::POP));
    }
    end_scope();
}

void Compiler::statement() {
    if (match(Token::Type::PRINT)) {
        print_statement();
    } else if (match(Token::Type::LEFT_BRACE)) {
        begin_scope();
        block();
        end_scope();
    } else if (match(Token::Type::IF)) {
        if_statement();
    } else if (match(Token::Type::WHILE)) {
        while_statement();
    } else if (match(Token::Type::FOR)) {
        for_statement();
    } else {
        expression_statement();
    }
}

void Compiler::synchronize() {
    panic_mode = false;
    while (current.type != Token::Type::END) {
        if (previous.type == Token::Type::SEMICOLON) return;
        switch (current.type) {
            case Token::Type::CLASS:
            case Token::Type::FUN:
            case Token::Type::VAR:
            case Token::Type::FOR:
            case Token::Type::IF:
            case Token::Type::WHILE:
            case Token::Type::PRINT:
            case Token::Type::RETURN:
                return;
            default:
                break;
        }
        advance();
    }
}

uint8_t Compiler::identifier_constant(const Token &token) {
    return make_constant(Value::make_object(vm.allocate_string(std::string(token.lexeme))));
}

void Compiler::add_local(const Token &name) {
    if (local_count == INT8_MAX + 1) {
        error("Too many local variables in function.");
        return;
    }

    Local& local = locals[local_count++];
    local.name = name;
    local.depth = -1;
}

void Compiler::declare_variable() {
    if (scope_depth == 0) return;
    for (int i = local_count - 1; i >= 0; i--) {
        Local local = locals[i];
        if (local.depth != -1 && local.depth < scope_depth) {
            break;
        }
        if (previous.lexeme == local.name.lexeme) {
            error("Already a variable with this name in this scope.");
        }
    }

    add_local(previous);
}

uint8_t Compiler::parse_variable(std::string_view error_message) {
    consume(Token::Type::IDENTIFIER, error_message);

    declare_variable();
    if (scope_depth > 0) return 0;

    return identifier_constant(previous);
}

void Compiler::mark_initialized() {
    locals[local_count - 1].depth = scope_depth;
}

void Compiler::define_variable(uint8_t global) {
    if (scope_depth > 0) {
        mark_initialized();
        return;
    }

    emit_bytes({static_cast<uint8_t>(Instruction::OpCode::DEFINE_GLOBAL), global});
}

void Compiler::var_declaration() {
    uint8_t global = parse_variable("Expect variable name.");

    if (match(Token::Type::EQUAL)) {
        expression();
    } else {
        emit_byte(static_cast<uint8_t>(Instruction::OpCode::NIL));
    }

    consume(Token::Type::SEMICOLON, "Expect ';' after variable declaration.");

    define_variable(global);
}

void Compiler::declaration() {
    if (match(Token::Type::VAR)) {
        var_declaration();
    } else {
        statement();
    }

    if (panic_mode) synchronize();
}

bool Compiler::compile() {
    had_error = false;
    panic_mode = false;

    advance();

    while (!match(Token::Type::END)) {
        declaration();
    }

    emit_byte(static_cast<uint8_t>(Instruction::OpCode::RETURN));

    // TODO remove
    if (!had_error) {
        debug::Disassembler disassembler("code", chunk);
        disassembler.disassemble();
    }

    return !had_error;
}

void Compiler::emit_byte(uint8_t byte) {
    chunk.write(byte, previous.line);
}

void Compiler::emit_bytes(const std::vector<uint8_t>& bytes) {
    for (const uint8_t byte : bytes) {
        emit_byte(byte);
    }
}
