#include "VM.h"

#include <iostream>
#include <algorithm>

// TODO: maybe add asserts

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

void VM::jump_to(uint32_t pos) {
    frames.back().instruction_pointer = pos;
}

Value VM::get_constant(const int idx) const {
    return frames.back().closure->get_function()->get_constant(idx);
}

uint32_t VM::get_jump_destination(const int idx) const {
    return frames.back().closure->get_function()->get_jump_destination(idx);
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
    std::optional<Object *> object = value.as<Object *>();

    if (!object) {
        return RuntimeError("Expected callable value such as function or class.");
    }

    if (auto *klass = dynamic_cast<Class *>(*object)) {
        // TODO: do this some other way as it definitely breaks gc
        std::vector<Value> args;
        for (int i = 0; i < arguments_count; ++i) {
            args.push_back(pop());
        }
        pop(); // pop class
        auto *instance = new Instance(klass);
        push(instance);
        allocate(instance);
        for (int i = arguments_count - 1; i >= 0; --i) {
            push(args[i]);
        }
        // TODO: handle private construtors?
        if (klass->methods.contains("init")) {
            return call_value(klass->methods["init"].value, arguments_count);
        }
        return {}; // success
    }
    if (auto *closure = dynamic_cast<Closure *>(*object)) {
        if (arguments_count != closure->get_function()->get_arity()) {
            return RuntimeError(std::format("Expected {} but got {} arguments",
                                            closure->get_function()->get_arity(), arguments_count));
        }
        frames.emplace_back(closure, 0, stack_index - arguments_count - 1);
        return {}; // success
    }
    if (auto *bound = dynamic_cast<BoundMethod *>(*object)) {
        // reuse closure value in stack as bound method reciver ("this")
        stack[stack_index - arguments_count - 1] = bound->receiver;
        return call_value(bound->closure, arguments_count);
    }

    if (auto* native = dynamic_cast<NativeFunction*>(*object)) {
        std::vector<Value> args;
        for (int i = 0; i < arguments_count + 1; ++i) {
            args.push_back(stack[stack_index - arguments_count - 1 + i]);
        }
        Value result = native->function(args);
        stack_index -= arguments_count + 1;
        push(result);
        return {};
    }
    return RuntimeError("Expected callable value such as function or class.");
}

Upvalue *VM::capture_upvalue(int index) {
    auto *value = &get_from_slot(index);
    // try to reuse already open upvalue
    Upvalue *upvalue = nullptr;
    for (auto open: open_upvalues) {
        if (open->location == value) {
            upvalue = open;
            break;
        }
    }
    if (upvalue == nullptr) {
        upvalue = new Upvalue(value);
        open_upvalues.push_back(upvalue);
        allocate<Upvalue>(upvalue);
    }
    return upvalue;
}

void VM::close_upvalues(const Value &value) {
    std::erase_if(open_upvalues, [&value](Upvalue *open) {
        if (open->location >= &value) {
            // maybe don't use memory addresses for this? seems unsafe
            open->closed = *open->location;
            open->location = &open->closed;
            return true;
        }
        return false;
    });
}

void VM::mark_roots_for_gc() {
    for (int i = 0; i < stack_index; ++i) {
        gc.mark(stack[i]);
    }

    for (auto &frame: frames) {
        gc.mark(frame.closure);
    }

    for (auto *open_upvalue: open_upvalues) {
        gc.mark(open_upvalue);
    }
    for (auto& value: std::views::values(natives)) {
        gc.mark(value);
    }
}

void VM::run_gc() {
    mark_roots_for_gc();
    gc.collect();
}

void VM::adopt_objects(std::vector<Object *> objects) {
    for (auto *object: objects) {
        allocate<Object>(object);
    }
}

bool VM::bind_method(Class *klass, const std::string &name) {
    if (!klass->methods.contains(name)) return false;
    auto* method = dynamic_cast<Closure *>(klass->methods[name].value.get<Object*>());
    auto* bound = new BoundMethod(peek(), method);
    pop();
    push(bound);
    allocate<BoundMethod>(bound);
    return true;
}

std::optional<Instance*> VM::get_current_instance() {
    Value first_on_stack = stack[frames.back().frame_pointer];
    if (auto object = first_on_stack.as<Object*>()) {
        if (auto* instance = dynamic_cast<Instance*>(*object)) {
            return instance;
        }
    }
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
                push(get_from_slot(idx));
                break;
            }
            case OpCode::SET: {
                int idx = fetch();
                set_in_slot(idx, peek());
                break;
            }
            case OpCode::JUMP_IF_FALSE: {
                int idx = fetch();
                if (peek().is_falsey()) {
                    jump_to(get_jump_destination(idx));
                }
                break;
            }
            case OpCode::JUMP_IF_TRUE: {
                int idx = fetch();
                if (!peek().is_falsey()) {
                    jump_to(get_jump_destination(idx));
                }
                break;
            }
            case OpCode::JUMP: {
                int idx = fetch();
                jump_to(get_jump_destination(idx));
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
                Function *function = reinterpret_cast<Function *>(*get_constant(fetch()).as<Object *>());
                auto *closure = new Closure(function);
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
                allocate<Closure>(closure); // wait to add upvalues
                // adopt_objects(function->get_allocated());
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
                auto *klass = new Class(name);
                push(klass);
                allocate(klass);
                break;
            }
            case OpCode::GET_PROPERTY: {
                std::optional<Object*> object = peek().as<Object*>();
                if (!object) {
                    return std::unexpected(RuntimeError("Expected class instance value."));
                }
                if (auto *instance = dynamic_cast<Instance *>(*object)) {
                    int constant_idx = fetch();
                    auto name = get_constant(constant_idx).get<std::string>();
                    if (instance->properties.contains(name)) {
                        pop();
                        ClassValue class_value = instance->properties[name];
                        std::optional<Instance*> current_instance = get_current_instance();
                        if (class_value.is_private && (!current_instance || *current_instance != instance)) {
                            return std::unexpected(RuntimeError("Attempted to read private property outside class definition"));
                        }
                        if (class_value.is_static) {
                            return std::unexpected(RuntimeError("Attempted to read static field on an instance."));
                        }
                        push(class_value.value);
                    } else if (instance->klass->methods.contains(name)) {
                        ClassValue class_value = instance->klass->methods[name];
                        std::optional<Instance*> current_instance = get_current_instance();
                        if (class_value.is_private && (!current_instance || *current_instance != instance)) {
                            return std::unexpected(RuntimeError("Attempted to read private property outside class definition"));
                        }
                        if (class_value.is_static) {
                            return std::unexpected(RuntimeError("Attempted to read static field on an instance."));
                        }
                        auto* method = dynamic_cast<Closure *>(class_value.value.get<Object*>());
                        auto* bound = new BoundMethod(peek(), method);
                        pop();
                        push(bound);
                        allocate<BoundMethod>(bound);
                    } else {
                        return std::unexpected(RuntimeError("Attempted to read not declared property."));
                    }
                } else if (auto* klass = dynamic_cast<Class*>(*object)) {
                    int constant_idx = fetch();
                    auto name = get_constant(constant_idx).get<std::string>();
                    if (klass->fields.contains(name)) {
                        pop();
                        ClassValue class_value = klass->fields[name];
                        // TODO: check private!
                        std::optional<Instance*> current_instance = get_current_instance();
                        if (class_value.is_private && (!current_instance || (*current_instance)->klass != klass)) {
                            return std::unexpected(RuntimeError("Attempted to read private property outside class definition"));
                        }
                        if (!class_value.is_static) {
                            return std::unexpected(RuntimeError("Attempted to read non-static field on a Class."));
                        }
                        push(class_value.value);
                    } else if (klass->methods.contains(name)) {
                        // TODO: check private!
                        ClassValue class_value = klass->methods[name];
                        std::optional<Instance*> current_instance = get_current_instance();
                        if (class_value.is_private && (!current_instance || (*current_instance)->klass != klass)) {
                            return std::unexpected(RuntimeError("Attempted to read private property outside class definition"));
                        }
                        if (!class_value.is_static) {
                            return std::unexpected(RuntimeError("Attempted to call non-static method on a Class."));
                        }
                        auto* method = dynamic_cast<Closure *>(class_value.value.get<Object*>());
                        auto* bound = new BoundMethod(peek(), method);
                        pop();
                        push(bound);
                        allocate<BoundMethod>(bound);
                    } else {
                        return std::unexpected(RuntimeError("Attempted to read not declared property."));
                    }
                } else {
                    return std::unexpected(RuntimeError("Expected class instance value."));
                }

                break;
            }
            case OpCode::SET_PROPERTY: {
                std::optional<Object*> object = peek(0).as<Object*>();
                if (!object) {
                    return std::unexpected(RuntimeError("Expected class instance value."));
                }
                if (auto *instance = dynamic_cast<Instance *>(*object)) {
                    int constant_idx = fetch();
                    auto name = get_constant(constant_idx).get<std::string>();
                    if (instance->properties.contains(name)) {
                        ClassValue class_value = instance->properties[name];
                        std::optional<Instance*> current_instance = get_current_instance();
                        if (class_value.is_private && (!current_instance || *current_instance != instance)) {
                            return std::unexpected(RuntimeError("Attempted to set private property outside class definition"));
                        }
                        if (class_value.is_static) {
                            return std::unexpected(RuntimeError("Attempted to set static field on an instance."));
                        }
                        instance->properties[name].value = peek(1);
                        Value value = pop();
                        pop(); // pop instance
                        push(value);
                    } else {
                        return std::unexpected(RuntimeError("Attempted to set not declared property."));
                    }
                } else if (auto* klass = dynamic_cast<Class*>(*object)) {
                    int constant_idx = fetch();
                    auto name = get_constant(constant_idx).get<std::string>();
                    if (klass->fields.contains(name)) {
                        ClassValue class_value = klass->fields[name];
                        std::optional<Instance*> current_instance = get_current_instance();
                        if (class_value.is_private && (!current_instance || (*current_instance)->klass != klass)) {
                            return std::unexpected(RuntimeError("Attempted to set private property outside class definition"));
                        }
                        if (!class_value.is_static) {
                            return std::unexpected(RuntimeError("Attempted to set non-static field on an Class."));
                        }
                        klass->fields[name].value = peek(1);
                        Value value = pop();
                        pop(); // pop instance
                        push(value);
                    } else {
                        return std::unexpected(RuntimeError("Attempted to set not declared property."));
                    }
                } else {
                    return std::unexpected(RuntimeError("Expected class instance value."));
                }
                break;
            }
            case OpCode::METHOD: {
                int constant_idx = fetch();
                int attributes = fetch();
                bool is_private = attributes & 1;
                bool is_static = attributes & 2;
                auto name = get_constant(constant_idx).get<std::string>();
                Value method = peek();
                Class *klass = dynamic_cast<Class *>(peek(1).get<Object *>());
                klass->methods[name] = {.value = method, .is_private = is_private, .is_static = is_static};
                pop();
                break;
            }
            case OpCode::INHERIT: {
                Class *superclass = dynamic_cast<Class *>(peek(1).get<Object *>());
                Class *subclass = dynamic_cast<Class *>(peek(0).get<Object *>());
                for (auto &value: superclass->methods) {
                    subclass->methods.insert(value);
                }
                for (auto& value : superclass->fields) {
                    subclass->fields.insert(value);
                }
                pop();
                break;
            }
            case OpCode::GET_SUPER: {
                int constant_idx = fetch();
                auto name = get_constant(constant_idx).get<std::string>();
                Class *superclass = dynamic_cast<Class *>(peek().get<Object *>());
                if (!superclass->methods.contains(name)) {
                    return std::unexpected(RuntimeError("Unexpected call to undefined method of super class."));
                }
                auto* method = dynamic_cast<Closure *>(superclass->methods[name].value.get<Object*>());
                auto* bound = new BoundMethod(peek(1), method);
                pop();
                push(bound);
                allocate<BoundMethod>(bound);
                break;
            }
            case OpCode::GET_NATIVE: {
                int constant_idx = fetch();
                auto name = get_constant(constant_idx).get<std::string>();
                push(natives[name]);
                break;
            }
            case OpCode::FIELD: {
                int constant_idx = fetch();
                int attributes = fetch();
                bool is_private = attributes & 1;
                bool is_static = attributes & 2;
                auto name = get_constant(constant_idx).get<std::string>();
                Value value = peek();
                Class *klass = dynamic_cast<Class *>(peek(1).get<Object *>());
                klass->fields[name] = {.value = value, .is_private = is_private, .is_static = is_static};
                pop();
                break;
            }
        }
        // for (int i = 0; i < stack_index; ++i) {
        //     std::cout << '[' << stack[i].to_string() << "] ";
        // }
        // std::cout << '\n';
    }
#undef BINARY_OPERATION
}

void VM::add_native(const std::string &name, const Value &value) {
    natives[name] = value;
}

void VM::add_native_function(const std::string &name, const std::function<Value(const std::vector<Value>&)> &fn) {
    auto* ptr = new NativeFunction(fn);
    natives[name] = ptr;
    allocate(ptr);
}