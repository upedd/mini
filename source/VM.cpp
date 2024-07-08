#include "VM.h"

#include <iostream>
#include <algorithm>

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
    auto* object = value.get<Object*>();

    if (auto* klass = dynamic_cast<Class*>(object)) {
        pop();
        auto* instance = new Instance(klass);
        push(instance);
        allocate(instance);
        if (klass->methods.contains("init")) {
            call_value(klass->methods["init"], arguments_count);
        }
    } else if (auto* closure = dynamic_cast<Closure*>(object)) {
        // if (arguments_count != closure->get_function()->get_arity()) {
        //     return RuntimeError(std::format("Expected {} but got {} arguments",
        //                                     closure->get_function()->get_arity(), arguments_count));
        // }
        frames.emplace_back(closure, 0, stack_index - arguments_count - 1);
    } else if (auto* bound = dynamic_cast<BoundMethod*>(object)) {
        stack[stack_index - arguments_count - 1] = bound->receiver;
        auto res = call_value(bound->closure, arguments_count);
        //push(bound->receiver);
    }
    // std::optional<Closure *> closure = reinterpret_cast<Closure*>(*value.as<Object *>());
    // if (!closure) {
    //     return RuntimeError("Expected callable value such as function or class");
    // }


    return {};
}

Upvalue *VM::capture_upvalue(int index) {
    auto *value = &get_from_slot(index);
    Upvalue *upvalue = nullptr;
    for (auto open: open_upvalues) {
        if (open->location == value) {
            upvalue = open;
            break;
        }
    }
    if (upvalue == nullptr) {
        upvalue = new Upvalue(value);
        open_upvalues.emplace(upvalue);
        allocate<Upvalue>(upvalue);
    }
    return upvalue;
}

void VM::close_upvalues(const Value &value) {
    std::vector<Upvalue*> to_erase;

    for (auto* open: open_upvalues) {
        if (open->location >= &value) {
            open->closed = *open->location;
            open->location = &open->closed;
            to_erase.push_back(open);
        }
    }
    for (auto* open : to_erase) {
        open_upvalues.erase(std::find(open_upvalues.begin(), open_upvalues.end(), open));
    }
}

void VM::mark_roots_for_gc() {
    for (int i = 0; i < stack_index; ++i) {
        gc.mark(stack[i]);
    }

    for (auto &frame: frames) {
        gc.mark(frame.closure);
    }

    for (auto* open_upvalue: open_upvalues) {
        gc.mark(open_upvalue);
    }
}

void VM::run_gc() {
    mark_roots_for_gc();
    gc.collect();
}

void VM::adopt_objects(std::vector<Object *> objects) {
    for (auto* object : objects) {
        allocate<Object>(object);
    }
}

bool VM::bind_method(Class *klass, const std::string &name) {
    Value method = klass->methods[name]; // TODO: check
    auto* bound = new BoundMethod(peek(), dynamic_cast<Closure*>(method.get<Object*>()));
    pop();
    push(bound);
    allocate<BoundMethod>(bound);
    return true;
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
                Function *function = reinterpret_cast<Function*>(*get_constant(fetch()).as<Object*>());
                auto *closure = new Closure(function);
                push(closure);
                adopt_objects(function->get_allocated());
                for (int i = 0; i < closure->get_function()->get_upvalue_count(); ++i) {
                    int is_local = fetch();
                    int index = fetch();
                    if (is_local) {
                        closure->upvalues.push_back(capture_upvalue(index));
                    } else {
                        closure->upvalues.push_back(frames.back().closure->upvalues[index]);
                    }
                }
                allocate<Closure>(closure); // wait to add upvalues
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
            case OpCode::CLASS: {
                int constant_idx = fetch();
                auto name = get_constant(constant_idx).get<std::string>();
                auto* klass = new Class(name);
                push(klass);
                allocate(klass);
                break;
            }
            case OpCode::GET_PROPERTY: {
                // TODO: handle error!!!!
                auto* instance = dynamic_cast<Instance*>(peek().get<Object*>());
                int constant_idx = fetch();
                auto name = get_constant(constant_idx).get<std::string>();
                if (instance->fields.contains(name)) {
                    pop();
                    push(instance->fields[name]);
                    break;
                } else if(!bind_method(instance->klass, name)) {
                    return std::unexpected(RuntimeError("Undefined property!"));
                }
                break;
            }
            case OpCode::SET_PROPERTY: {
                auto* instance = dynamic_cast<Instance*>(peek(1).get<Object*>());
                int constant_idx = fetch();
                auto name = get_constant(constant_idx).get<std::string>();
                instance->fields[name] = peek(0);
                Value value = pop();
                pop(); // pop instance
                push(value);
                break;
            }
            case OpCode::METHOD: {
                int constant_idx = fetch();
                auto name = get_constant(constant_idx).get<std::string>();
                Value method = peek();
                Class* klass = dynamic_cast<Class*>(peek(1).get<Object*>());
                klass->methods[name] = method;
                pop();
                break;
            }
            case OpCode::INHERIT: {
                Class* superclass = dynamic_cast<Class*>(peek(1).get<Object*>());
                Class* subclass = dynamic_cast<Class*>(peek(0).get<Object*>());
                for (auto& value : superclass->methods) {
                    subclass->methods.insert(value);
                }
                pop();
                break;
            }
            case OpCode::GET_SUPER: {
                int constant_idx = fetch();
                auto name = get_constant(constant_idx).get<std::string>();
                Class* superclass = dynamic_cast<Class*>(peek().get<Object*>());
                bind_method(superclass, name);
                break;
            }
        }
        // for (int i = 0; i < stack_index; ++i) {
        //     std::cout << stack[i].to_string() << ' ';
        // }
        // std::cout << '\n';
    }
#undef BINARY_OPERATION
}
