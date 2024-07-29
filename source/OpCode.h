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
    POP,
    GET,
    SET,
    JUMP_IF_FALSE,
    JUMP,
    JUMP_IF_TRUE,
    NOT,
    BINARY_NOT,
    MODULO,
    FLOOR_DIVISON,
    CALL,
    RETURN,
    CLOSURE,
    GET_UPVALUE,
    SET_UPVALUE,
    CLOSE_UPVALUE,
    CLASS,
    GET_PROPERTY,
    SET_PROPERTY,
    METHOD,
    INHERIT,
    GET_SUPER,
    GET_NATIVE,
    FIELD,
    THIS,
    CALL_SUPER_CONSTRUCTOR,
    CONSTRUCTOR,
    ABSTRACT_CLASS,
    SET_SUPER,
    TRAIT,
    TRAIT_METHOD,
    GET_TRAIT,
};

#endif //OPCODE_H
