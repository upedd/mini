//
// Created by MiłoszK on 23.06.2024.
//

#ifndef COMPILER_H
#define COMPILER_H
#include <string_view>

#include "Chunk.h"
#include "Token.h"
#include "Scanner.h"

namespace vm {
    class Compiler {
    public:
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

        using ParseFn = void(Compiler::*)();

        class ParseRule {
        public:
            ParseFn prefix;
            ParseFn infix;
            Precedence precedence;
        };


        ParseRule *get_rule(Token::Type type);

        explicit Compiler(const std::string_view source) : scanner(source) {
        }

        void error(std::string_view message);

        void error_at(const Token &token, std::string_view message);

        void error_at_current(std::string_view message);

        void advance();

        void consume(Token::Type type, std::string_view message);

        void expression();

        void grouping();

        void unary();

        void binary();

        void parse_precendence(Precedence precendence);

        uint8_t make_constant(Value value);

        void emit_constant(Value value);

        void number();

        bool compile();

        void emit_byte(uint8_t byte);

        void emit_bytes(const std::vector<uint8_t> &bytes);
        Chunk chunk;
    private:

        Token current;
        Token previous;
        Scanner scanner;
        bool had_error = false;
        bool panic_mode = false;

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
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {number, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
        };
    };
}


#endif //COMPILER_H
