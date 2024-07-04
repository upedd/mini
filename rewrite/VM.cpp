#include "VM.h"

#include <iostream>


uint8_t VM::fetch() {
    CallFrame &frame = frames.back();
    return frame.closure->get_function()->get_program().get_at(frame.instruction_pointer++);
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
    return frames.back().closure->get_function()->get_constant(idx);
}

Value VM::pop() {
    auto value = peek();
    --stack_index;
    return value;
}

Value VM::peek(const int n) const {
    return stack[stack_index - n - 1];
}

void VM::push(const Value &value) {
    stack[stack_index++] = value;
}

Value &VM::get_from_slot(int index) {
    return stack[frames.back().frame_pointer + index];
}

void VM::set_in_slot(const int index, const Value &value) {
    stack[frames.back().frame_pointer + index] = value;
}


std::optional<VM::RuntimeError> VM::call_value(const Value &value, const int arguments_count) {
    // todo: refactor!
    std::optional<Closure*> closure = value.as<Closure*>();
    if (!closure) {
        return RuntimeError("Expected callable value such as function or class");
    }

    if (arguments_count != closure.value()->get_function()->get_arity()) {
        return RuntimeError(std::format("Expected {} but got {} arguments", closure.value()->get_function()->get_arity(), arguments_count));
    }
    frames.emplace_back(*closure, 0, stack_index - arguments_count - 1);
    return {};
}

Upvalue * VM::capture_upvalue(int index) {
    auto* value = &get_from_slot(index);
    Upvalue* upvalue = nullptr;
    for (auto open : open_upvalues) {
        if (open->location == value) {
            upvalue = open;
            break;
        }
    }
    if (upvalue == nullptr) {
        upvalue = new Upvalue(value);
        open_upvalues.emplace(upvalue);
    }
    return upvalue;
}

void VM::close_upvalues(const Value &value) {
    for (auto open : open_upvalues) {
        if (open->location >= &value) {
            open->closed = *open->location;
            open->location = &open->closed;
        }
    }
}

void VM::mark_object(Object* object) {
    if (object == nullptr) return;
    object->is_marked = true;
    gray_objects.push_back(object);
#ifdef DEBUG_LOG_GC
    std::cout << "Marked object.\n"; // todo: better debug info!
#endif
}

void VM::mark_value(const Value &value) {
    if (value.is<Object*>()) {
        mark_object(value.get<Object*>());
    }
}

void VM::mark_roots() {
    for (int i = 0; i <= stack_index; ++i) {
        mark_value(stack[i]);
    }

    for (auto& frame : frames) {
        mark_object(frame.closure);
    }

    for (auto& open_upvalue : open_upvalues) {
        mark_object(open_upvalue);
    }
    // TODO!!! mark compiler objects
}

void VM::blacken_object(Object *object) {
    // TODO
}

void VM::trace_references() {
    while (!gray_objects.empty()) {
        Object* object = gray_objects.back(); gray_objects.pop_back();
        blacken_object(object);
    }
}

void VM::collect_garbage() {
#ifdef DEBUG_LOG_GC
    std::cout << "--- gc start\n";
#endif

    mark_roots();
    trace_references();

#ifdef DEBUG_LOG_GC
    std::cout << "--- gc end\n";
#endif
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
                break;
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
                close_upvalues(stack[frames.back().frame_pointer]); // TODO: check
                while (stack_index > frames.back().frame_pointer) {
                    pop();
                }
                frames.pop_back();
                push(result);
                if (frames.empty()) return pop();
                break;
            }
            case OpCode::CLOSURE: {
                Function* function = *get_constant(fetch()).as<Function*>();
                auto* closure = new Closure(function); // mem leak
                push(closure);
                for (int i = 0; i < closure->get_function()->get_upvalue_count(); ++i) {
                    int is_local = fetch();
                    int index = fetch();
                    if (is_local) {
                        closure->upvalues.push_back(capture_upvalue(index));
                    } else {
                        closure->upvalues.push_back(frames.back().closure->upvalues[index]);
                    }
                }
                break;
            }
            case OpCode::GET_UPVALUE: {
                int slot = fetch();
                push(*frames.back().closure->upvalues[slot]->location);
                break;
            }
            case OpCode::SET_UPVALUE: {
                int slot = fetch();
                *frames.back().closure->upvalues[slot]->location = peek();
                break;
            }
            case OpCode::CLOSE_UPVALUE: {
                close_upvalues(peek());
                pop();
                break;
            }
        }
        for (int i = 0; i < stack_index; ++i) {
            std::cout << stack[i].to_string() << ' ';
        }
        std::cout << '\n';
    }
#undef BINARY_OPERATION
}
