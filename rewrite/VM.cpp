#include "VM.h"

#include <iostream>

void VM::tick() {
    OpCode code = reader.opcode();
    switch (code) {
        case OpCode::CONSTANT: {
            int8_t index = reader.read();
            stack.push_back(reader.get_constant(index));
            break;
        }
        case OpCode::ADD: {
            int64_t b = std::get<int64_t>(stack.back()); stack.pop_back();
            int64_t a = std::get<int64_t>(stack.back()); stack.pop_back();
            stack.emplace_back(a + b);
            break;
        }
        case OpCode::MULTIPLY: {
            int64_t b = std::get<int64_t>(stack.back()); stack.pop_back();
            int64_t a = std::get<int64_t>(stack.back()); stack.pop_back();
            stack.emplace_back(a * b);
            break;
        }
        case OpCode::SUBTRACT: {
            int64_t b = std::get<int64_t>(stack.back()); stack.pop_back();
            int64_t a = std::get<int64_t>(stack.back()); stack.pop_back();
            stack.emplace_back(a - b);
            break;
        }
        case OpCode::DIVIDE: {
            int64_t b = std::get<int64_t>(stack.back()); stack.pop_back();
            int64_t a = std::get<int64_t>(stack.back()); stack.pop_back();
            stack.emplace_back(a / b);
            break;
        }
        case OpCode::NEGATE: {
            // optim just negate top?
            int64_t val = std::get<int64_t>(stack.back()); stack.pop_back();
            stack.emplace_back(-val);
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
}

void VM::run() {
    while (!reader.at_end()) {
        tick();

        // todo temp debug
        for (auto& x : stack) {
            std::cout << value_to_string(x) << ' ';
        }
        std::cout << '\n';
    }
}
