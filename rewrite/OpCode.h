#ifndef OPCODE_H
#define OPCODE_H

#include "types.h"

enum class OpCode : bite_byte {
    ADD,
    MULTIPLY,
    SUBTRACT,
    DIVIDE,
    NEGATE,
    TRUE,
    FALSE,
    NIL,
    CONSTANT,
    EQUAL,
    NOT_EQUAL,
    LESS,
    LESS_EQUAL,
    GREATER,
    GREATER_EQUAL,
    LEFT_SHIFT,
    RIGHT_SHIFT,
    BITWISE_AND,
    BITWISE_OR,
    BITWISE_XOR,
    POP, GET, SET, JUMP_IF_FALSE, JUMP, LOOP, JUMP_IF_TRUE, NOT, BINARY_NOT, MODULO, FLOOR_DIVISON, CALL, RETURN
};

#endif //OPCODE_H
