#ifndef OPCODE_H
#define OPCODE_H
#include <cstdint>

enum class OpCode : uint8_t {
    PUSH_INT,
    ADD,
    MULTIPLY,
    SUBTRACT,
    DIVIDE,
    NEGATE
};

#endif //OPCODE_H
