//
// Created by MiłoszK on 23.06.2024.
//

#ifndef INSTRUCTION_H
#define INSTRUCTION_H
#include <cstdint>
namespace vm {
    class Instruction {
    public:
        enum class OpCode : uint8_t {
            RETURN,
            CONSTANT,
            NIL,
            TRUE,
            FALSE,
            NEGATE,
            EQUAL,
            GREATER,
            LESS,
            ADD,
            SUBTRACT,
            MULTIPLY,
            DIVIDE,
            NOT, PRINT, POP,
            DEFINE_GLOBAL, GET_GLOBAL, SET_GLOBAL
        };

        Instruction(const OpCode op_code, int line) : op_code(op_code), line(line) {}



        [[nodiscard]] OpCode get_op_code() const {
            return op_code;
        }

        [[nodiscard]] int get_line() const {
            return line;
        }

    private:
        OpCode op_code;
        int line;
    };
}


#endif //INSTRUCTION_H
