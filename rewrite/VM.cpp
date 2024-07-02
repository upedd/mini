#include "VM.h"

#include <iostream>

#include "Function.h"


void VM::call_value(Value &value, int arguments_count) {
    Function* function = std::get<Function*>(value); // error check!
    frames.emplace_back(function, 0, stack.size() - arguments_count - 1);
}

void VM::tick() {
    auto& frame = frames.back();
    reader.set_frame(frame);

#define BINARY_OPERATION(op) { \
    auto b = stack.back(); \
    stack.pop_back(); \
    auto a = stack.back(); \
    stack.pop_back(); \
    stack.emplace_back(a.op(b)); \
    break; \
}

    OpCode code = reader.opcode();
    switch (code) {
        case OpCode::CONSTANT: {
            int8_t index = reader.read();
            stack.push_back(reader.get_constant(index));
            break;
        }
        case OpCode::ADD: BINARY_OPERATION(add)
        case OpCode::MULTIPLY: BINARY_OPERATION(multiply)
        case OpCode::SUBTRACT: BINARY_OPERATION(subtract)
        case OpCode::DIVIDE: BINARY_OPERATION(divide)
        case OpCode::EQUAL: BINARY_OPERATION(equals)
        case OpCode::NOT_EQUAL: BINARY_OPERATION(not_equals)
        case OpCode::LESS: BINARY_OPERATION(less)
        case OpCode::LESS_EQUAL: BINARY_OPERATION(less_equal)
        case OpCode::GREATER: BINARY_OPERATION(greater)
        case OpCode::GREATER_EQUAL: BINARY_OPERATION(greater_equal)
        case OpCode::RIGHT_SHIFT: BINARY_OPERATION(shift_left)
        case OpCode::LEFT_SHIFT: BINARY_OPERATION(shift_right)
        case OpCode::BITWISE_AND: BINARY_OPERATION(binary_and)
        case OpCode::BITWISE_OR: BINARY_OPERATION(binary_or)
        case OpCode::BITWISE_XOR: BINARY_OPERATION(binary_xor)
        case OpCode::MODULO: BINARY_OPERATION(modulo)
        case OpCode::FLOOR_DIVISON: BINARY_OPERATION(floor_divide)
        case OpCode::NEGATE: {
            // optim just negate top?
            auto top = stack.back();
            stack.pop_back();
            stack.emplace_back(top.multiply(-1));
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
        case OpCode::POP: {
            stack.pop_back();
            break;
        }
        case OpCode::GET: {
            int idx = reader.read();
            stack.push_back(stack[frame.frame_pointer + idx]); // handle overflow?
            break;
        }
        case OpCode::SET: {
            int idx = reader.read();
            stack[frame.frame_pointer + idx] = stack.back();
            break;
        }
        case OpCode::JUMP_IF_FALSE: {
            int offset = (static_cast<int>(reader.read()) << 8) | static_cast<int>(reader.read());
            // todo better check
            bool cond = std::get<bool>(stack.back());
            if (!cond) {
                reader.add_offset(offset);
            }
            break;
        }
        case OpCode::JUMP_IF_TRUE: {
            int offset = (static_cast<int>(reader.read()) << 8) | static_cast<int>(reader.read());
            // todo better check
            bool cond = std::get<bool>(stack.back());
            if (cond) {
                reader.add_offset(offset);
            }
            break;
        }
        case OpCode::JUMP: {
            int offset = (static_cast<int>(reader.read()) << 8) | static_cast<int>(reader.read());
            reader.add_offset(offset);
            break;
        }
        case OpCode::LOOP: {
            int offset = (static_cast<int>(reader.read()) << 8) | static_cast<int>(reader.read());
            reader.add_offset(-offset);
            break;
        }
        case OpCode::NOT: {
            bool cond = std::get<bool>(stack.back()); stack.pop_back();
            stack.emplace_back(!cond);
            break;
        }
        case OpCode::BINARY_NOT: {
            auto value = stack.back(); stack.pop_back();
            stack.emplace_back(value.binary_not());
        }
        case OpCode::CALL: {
            int arguments_count = reader.read();
            call_value(stack[stack.size() - arguments_count - 1], arguments_count);
            reader.set_frame(frames.back());
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
