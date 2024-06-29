#ifndef OPCODE_H
#define OPCODE_H
#include <cstdint>

enum class OpCode : uint8_t {
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
    GREATER_EQUAL
};

#endif //OPCODE_H
