#include "VM.h"

#include <iostream>



void VM::tick() {

#define NUMBER_BINARY_OPERATION(op) { \
    auto b = stack.back(); \
    stack.pop_back(); \
    auto a = stack.back(); \
    stack.pop_back(); \
    if (!a.is_number() || !b.is_number()) { \
        throw RuntimeError("Both operands must be convertible to numbers"); \
    }\
    stack.emplace_back(a.get_number() op b.get_number()); \
    break; \
}

    OpCode code = reader.opcode();
    switch (code) {
        case OpCode::CONSTANT: {
            int8_t index = reader.read();
            stack.push_back(reader.get_constant(index));
            break;
        }
        case OpCode::ADD: NUMBER_BINARY_OPERATION(+)
        case OpCode::MULTIPLY: NUMBER_BINARY_OPERATION(*)
        case OpCode::SUBTRACT: NUMBER_BINARY_OPERATION(*)
        case OpCode::DIVIDE: NUMBER_BINARY_OPERATION(*)
        // case OpCode::EQUAL: {
        //     auto b = stack.back();
        //     stack.pop_back();
        //     auto a = stack.back();
        //     stack.pop_back();
        //     stack.emplace_back(a == b);
        //     break;
        // }
        // case OpCode::NOT_EQUAL: {
        //     int64_t b = std::get<int64_t>(stack.back());
        //     stack.pop_back();
        //     int64_t a = std::get<int64_t>(stack.back());
        //     stack.pop_back();
        //     stack.emplace_back(a != b);
        //     break;
        // }
        // case OpCode::LESS: {
        //     int64_t b = std::get<int64_t>(stack.back());
        //     stack.pop_back();
        //     int64_t a = std::get<int64_t>(stack.back());
        //     stack.pop_back();
        //     stack.emplace_back(a < b);
        //     break;
        // }
        // case OpCode::LESS_EQUAL: {
        //     int64_t b = std::get<int64_t>(stack.back());
        //     stack.pop_back();
        //     int64_t a = std::get<int64_t>(stack.back());
        //     stack.pop_back();
        //     stack.emplace_back(a <= b);
        //     break;
        // }
        // case OpCode::GREATER: {
        //     int64_t b = std::get<int64_t>(stack.back());
        //     stack.pop_back();
        //     int64_t a = std::get<int64_t>(stack.back());
        //     stack.pop_back();
        //     stack.emplace_back(a > b);
        //     break;
        // }
        // case OpCode::GREATER_EQUAL: {
        //     int64_t b = std::get<int64_t>(stack.back());
        //     stack.pop_back();
        //     int64_t a = std::get<int64_t>(stack.back());
        //     stack.pop_back();
        //     stack.emplace_back(a >= b);
        //     break;
        // }
        // case OpCode::NEGATE: {
        //     // optim just negate top?
        //     int64_t val = std::get<int64_t>(stack.back());
        //     stack.pop_back();
        //     stack.emplace_back(-val);
        //     break;
        // }
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

#undef HYBRID_BINARY_OPERATION
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
