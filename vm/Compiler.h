//
// Created by MiłoszK on 23.06.2024.
//

#ifndef COMPILER_H
#define COMPILER_H
#include <string_view>

#include "Chunk.h"
#include "Token.h"
#include "Scanner.h"
#include "VM.h"

namespace vm {
    class Compiler {
    public:
        explicit Compiler(VM& vm, const std::string_view source) : vm(vm), scanner(source) {}

        bool check(Token::Type type);

        bool match(Token::Type type);

        void print_statement();

        void expression_statement();

        void statement();

        void synchronize();

        uint8_t identifier_constant(const Token & token);

        uint8_t parse_variable(std::string_view error_message);

        void define_variable(uint8_t global);

        void var_declaration();

        void declaration();

        bool compile();

        Chunk& get_chunk() { return chunk; }
    private:
        void error(std::string_view message);
        void error_at(const Token &token, std::string_view message);
        void error_at_current(std::string_view message);

        void advance();
        void consume(Token::Type type, std::string_view message);

        void expression();
        void grouping(bool can_assign);
        void unary(bool can_assign);
        void binary(bool can_assign);

        void number(bool can_assign);
        void string(bool can_assign);
        void literal(bool can_assign);

        void named_variable(const Token &name, bool can_assign);

        void variable(bool can_assign);

        uint8_t make_constant(Value value);
        void emit_constant(Value value);


        void emit_byte(uint8_t byte);
        void emit_bytes(const std::vector<uint8_t> &bytes);

        Chunk chunk;
        Token current;
        Token previous;
        Scanner scanner;
        VM& vm;

        bool had_error = false;
        bool panic_mode = false;

        enum class Precedence {
            NONE,
            ASSIGMENT,
            OR,
            AND,
            EQUALITY,
            COMPARISON,
            TERM,
            FACTOR,
            UNARY,
            CALL,
            PRIMARY
        };

        using ParseFn = void(Compiler::*)(bool);

        class ParseRule {
        public:
            ParseFn prefix;
            ParseFn infix;
            Precedence precedence;
        };

        ParseRule *get_rule(Token::Type type);

        ParseRule rules[40] = { // same order as in Token::Type!
            {grouping, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {unary, binary, Precedence::TERM},
            {nullptr, binary, Precedence::TERM},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, binary, Precedence::FACTOR},
            {nullptr, binary, Precedence::FACTOR},
            {unary, nullptr, Precedence::NONE},
            {nullptr, binary, Precedence::EQUALITY},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, binary, Precedence::EQUALITY},
            {nullptr, binary, Precedence::COMPARISON},
            {nullptr, binary, Precedence::COMPARISON},
            {nullptr, binary, Precedence::COMPARISON},
            {nullptr, binary, Precedence::COMPARISON},
            {variable, nullptr, Precedence::NONE},
            {string, nullptr, Precedence::NONE},
            {number, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {literal, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {literal, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {literal, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
        };

        void parse_precendence(Precedence precendence);
    };
}


#endif //COMPILER_H
