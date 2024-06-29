#include "VM.h"

#include <iostream>



void VM::tick() {

#define BINARY_OPERATION(op) { \
    auto b = stack.back(); \
    stack.pop_back(); \
    auto a = stack.back(); \
    stack.pop_back(); \
    stack.emplace_back(a op b); \
    break; \
}

    OpCode code = reader.opcode();
    switch (code) {
        case OpCode::CONSTANT: {
            int8_t index = reader.read();
            stack.push_back(reader.get_constant(index));
            break;
        }
        case OpCode::ADD: BINARY_OPERATION(+)
        case OpCode::MULTIPLY: BINARY_OPERATION(*)
        case OpCode::SUBTRACT: BINARY_OPERATION(-)
        case OpCode::DIVIDE: BINARY_OPERATION(/)
        case OpCode::EQUAL: BINARY_OPERATION(==)
        case OpCode::NOT_EQUAL: BINARY_OPERATION(!=)
        case OpCode::LESS: BINARY_OPERATION(<)
        case OpCode::LESS_EQUAL: BINARY_OPERATION(<=)
        case OpCode::GREATER: BINARY_OPERATION(>)
        case OpCode::GREATER_EQUAL: BINARY_OPERATION(>=)
        case OpCode::NEGATE: {
            // optim just negate top?
            auto top = stack.back();
            stack.pop_back();
            stack.emplace_back(top * -1);
            break;
        }
        case OpCode::TRUE: {
            stack.emplace_back(true);
            break;
        }
        case OpCode::FALSE: {
            stack.emplace_back(false);
            break;
        }
        case OpCode::NIL: {
            stack.emplace_back(nil_t);
            break;
        }
    }

#undef BINARY_OPERATION
}

void VM::run() {
    while (!reader.at_end()) {
        tick();

        // todo temp debug
        for (auto &x: stack) {
            std::cout << x.to_string() << ' ';
        }
        std::cout << '\n';
    }
}
