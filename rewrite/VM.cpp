#include "VM.h"

#include <iostream>

#include "Function.h"


uint8_t VM::fetch() {
    CallFrame &frame = frames.back();
    return frame.function->get_program().get_at(frame.instruction_pointer++);
}

OpCode VM::fetch_opcode() {
    return static_cast<OpCode>(fetch());
}

uint16_t VM::fetch_short() {
    return static_cast<uint16_t>(fetch()) << 8 | fetch();
}

void VM::jump_by_offset(int offset) {
    frames.back().instruction_pointer += offset;
}

Value VM::get_constant(const int idx) const {
    return frames.back().function->get_constant(idx);
}

Value VM::pop() {
    auto value = peek();
    stack.pop_back();
    return value;
}

Value VM::peek(const int n) const {
    return stack[stack.size() - n - 1];
}

void VM::push(const Value &value) {
    stack.push_back(value);
}

Value VM::get_from_slot(int index) {
    return stack[frames.back().frame_pointer + index];
}

void VM::set_in_slot(const int index, const Value &value) {
    stack[frames.back().frame_pointer + index] = value;
}


std::optional<VM::RuntimeError> VM::call_value(const Value &value, const int arguments_count) {
    std::optional<Function*> function = value.as<Function*>();
    if (!function) {
        return RuntimeError("Expected callable value such as function or class");
    }
    if (arguments_count != function.value()->get_arity()) {
        return RuntimeError(std::format("Expected {} but got {} arguments", function.value()->get_arity(), arguments_count));
    }
    frames.emplace_back(*function, 0, stack.size() - arguments_count - 1);
    return {};
}

std::expected<Value, VM::RuntimeError> VM::run() {
#define BINARY_OPERATION(op) { \
    auto b = pop(); \
    auto a = pop(); \
    push(a.op(b)); \
    break; \
}
    while (true) {

        switch (fetch_opcode()) {
            case OpCode::CONSTANT: {
                uint8_t index = fetch();
                push(get_constant(index));
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
                auto top = pop();
                push(top.multiply(-1));
                break;
            }
            case OpCode::TRUE: {
                push(true);
                break;
            }
            case OpCode::FALSE: {
                push(false);
                break;
            }
            case OpCode::NIL: {
                push(nil_t);
                break;
            }
            case OpCode::POP: {
                pop();
                break;
            }
            case OpCode::GET: {
                int idx = fetch();
                push(get_from_slot(idx)); // handle overflow?
                break;
            }
            case OpCode::SET: {
                int idx = fetch();
                set_in_slot(idx, peek());
                break;
            }
            case OpCode::JUMP_IF_FALSE: {
                int offset = fetch_short();
                if (peek().is_falsey()) {
                    jump_by_offset(offset);
                }
                break;
            }
            case OpCode::JUMP_IF_TRUE: {
                int offset = fetch_short();
                if (!peek().is_falsey()) {
                    jump_by_offset(offset);
                }
                break;
            }
            case OpCode::JUMP: {
                int offset = fetch_short();
                jump_by_offset(offset);
                break;
            }
            case OpCode::LOOP: {
                int offset = fetch_short();
                jump_by_offset(-offset);
                break;
            }
            case OpCode::NOT: {
                std::optional<bool> condition = pop().as<bool>();
                if (!condition)
                    return std::unexpected(RuntimeError("Negation is only supported on boolean type."));
                push(!condition.value());
                break;
            }
            case OpCode::BINARY_NOT: {
                auto value = pop();
                push(value.binary_not());
            }
            case OpCode::CALL: {
                int arguments_count = fetch();
                if (auto error = call_value(peek(arguments_count), arguments_count)) {
                    return std::unexpected(*error);
                }
                break;
            }
            case OpCode::RETURN: {
                Value result = pop();
                while (stack.size() > frames.back().frame_pointer) {
                    pop();
                }
                frames.pop_back();
                push(result);
                if (frames.empty()) return pop();
            }
        }
        // for (auto &x: stack) {
        //     std::cout << x.to_string() << ' ';
        // }
        // std::cout << '\n';
    }
#undef BINARY_OPERATION
}
