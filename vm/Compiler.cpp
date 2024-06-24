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

void Compiler::grouping() {
    expression();
    consume(Token::Type::RIGHT_PAREN, "Expect ')' after expression.");
}

void Compiler::unary() {
    Token::Type operator_type = previous.type;
    parse_precendence(Precedence::UNARY);
    switch (operator_type) {
        case Token::Type::MINUS: emit_byte(static_cast<uint8_t>(Instruction::OpCode::NEGATE)); break;
        case Token::Type::BANG: emit_byte(static_cast<uint8_t>(Instruction::OpCode::NOT)); break;
        default: return; // unreachable
    }
}

void Compiler::binary() {
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
        case Token::Type::LESS: emit_byte(static_cast<int>(Instruction::OpCode::GREATER)); break;
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

    (this->*prefix_rule)();

    while (precendence <= get_rule(current.type)->precedence) {
        advance();
        ParseFn infix_rule = get_rule(previous.type)->infix;
        (this->*infix_rule)();
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

void Compiler::number() {
    double value = std::stod(previous.lexeme.data());
    emit_constant(Value::make_number(value));
}

void Compiler::string() {
    // TODO should we dealoc literals?
    emit_constant(
        Value::make_object(
            vm.allocate_string(
                std::string(previous.lexeme.substr(1, previous.lexeme.size() - 2))
                )
            )
        );
}

void Compiler::literal() {
    switch (previous.type) {
        case Token::Type::FALSE: emit_byte(static_cast<uint8_t>(Instruction::OpCode::FALSE)); break;
        case Token::Type::NIL: emit_byte(static_cast<uint8_t>(Instruction::OpCode::NIL)); break;
        case Token::Type::TRUE: emit_byte(static_cast<uint8_t>(Instruction::OpCode::TRUE)); break;
        default: return;
    }
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

void Compiler::statement() {
    if (match(Token::Type::PRINT)) {
        print_statement();
    }
}

void Compiler::declaration() {
    statement();
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
