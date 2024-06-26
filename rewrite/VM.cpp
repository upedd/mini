#include "VM.h"

void VM::tick() {
    OpCode code = reader.opcode();
    switch (code) {
        case OpCode::PUSH_INT:
            stack.push_back(reader.integer());
        break;
        case OpCode::ADD: {
            int64_t b = stack.back(); stack.pop_back();
            int64_t a = stack.back(); stack.pop_back();
            stack.push_back(a + b);
            break;
        }
        case OpCode::MULTIPLY: {
            int64_t b = stack.back(); stack.pop_back();
            int64_t a = stack.back(); stack.pop_back();
            stack.push_back(a * b);
            break;
        }
        case OpCode::SUBTRACT: {
            int64_t b = stack.back(); stack.pop_back();
            int64_t a = stack.back(); stack.pop_back();
            stack.push_back(a - b);
            break;
        }
        case OpCode::DIVIDE: {
            int64_t b = stack.back(); stack.pop_back();
            int64_t a = stack.back(); stack.pop_back();
            stack.push_back(a / b);
            break;
        }
        case OpCode::NEGATE: {
            // optim just negate top?
            int64_t val = stack.back(); stack.pop_back();
            stack.push_back(-val);
            break;
        }
    }
}

void VM::run() {
    while (!reader.at_end()) {
        tick();
    }
}
