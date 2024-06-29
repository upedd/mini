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
    CONSTANT
};

#endif //OPCODE_H
