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
        explicit Compiler(const std::string_view source) : scanner(source) {}

        bool compile();

        Chunk& get_chunk() { return chunk; }
    private:
        void error(std::string_view message);
        void error_at(const Token &token, std::string_view message);
        void error_at_current(std::string_view message);

        void advance();
        void consume(Token::Type type, std::string_view message);

        void expression();
        void grouping();
        void unary();
        void binary();

        void number();
        void literal();

        uint8_t make_constant(Value value);
        void emit_constant(Value value);


        void emit_byte(uint8_t byte);
        void emit_bytes(const std::vector<uint8_t> &bytes);

        Chunk chunk;
        Token current;
        Token previous;
        Scanner scanner;

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

        using ParseFn = void(Compiler::*)();

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
            {nullptr, nullptr, Precedence::NONE},
            {nullptr, nullptr, Precedence::NONE},
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
