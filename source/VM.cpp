#include "VM.h"

#include <iostream>
#include <algorithm>

#include "shared/SharedContext.h"

// TODO: maybe add asserts

uint8_t VM::fetch() {
    CallFrame& frame = frames.back();
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

void VM::push(const Value& value) {
    stack[stack_index++] = value;
}

Value& VM::get_from_slot(int index) {
    return stack[frames.back().frame_pointer + index];
}

void VM::set_in_slot(const int index, const Value& value) {
    stack[frames.back().frame_pointer + index] = value;
}


std::optional<VM::RuntimeError> VM::call_value(const Value& value, const int arguments_count) {
    // todo: refactor!
    std::optional<Object*> object = value.as<Object*>();

    if (!object) {
        return RuntimeError("Expected callable value such as function or class.");
    }

    if (auto* klass = dynamic_cast<Class*>(*object)) {
        if (klass->is_abstract) {
            return RuntimeError("Cannot instantiate abstract class");
        }
        // TODO: do this some other way as it definitely breaks gc
        std::vector<Value> args;
        for (int i = 0; i < arguments_count; ++i) {
            args.push_back(pop());
        }
        pop(); // pop class
        auto* instance = new Instance(klass);
        push(instance);
        allocate(instance);
        auto bound = bind_method(klass->constructor, klass, instance);
        push(bound);
        // TODO: temp fix
        // work around gc and fix duplicate function returns
        if (auto* bound_ptr = dynamic_cast<BoundMethod*>(bound.get<Object*>())) {
            allocate(bound_ptr);
            allocate(dynamic_cast<Receiver*>(bound_ptr->receiver.get<Object*>()));
        }

        std::swap(stack[stack_index - 1], stack[stack_index - 2]);
        pop();

        for (int i = arguments_count - 1; i >= 0; --i) {
            push(args[i]);
        }
        // TODO: refactor when gc is refactored!

        auto res = call_value(bound, arguments_count); // success
        return res;
    }
    if (auto* closure = dynamic_cast<Closure*>(*object)) {
        if (arguments_count != closure->get_function()->get_arity()) {
            return RuntimeError(
                std::format("Expected {} but got {} arguments", closure->get_function()->get_arity(), arguments_count)
            );
        }
        frames.emplace_back(closure, 0, stack_index - arguments_count - 1);
        return {}; // success
    }
    if (auto* bound = dynamic_cast<BoundMethod*>(*object)) {
        // reuse closure value in stack as bound method reciver ("this")
        stack[stack_index - arguments_count - 1] = bound->receiver;
        return call_value(bound->closure, arguments_count);
    }

    if (auto* foreign = dynamic_cast<ForeginFunctionObject*>(*object)) {
        std::vector<Value> args;
        for (int i = 0; i < arguments_count + 1; ++i) {
            args.push_back(stack[stack_index - arguments_count - 1 + i]);
        }
        Value result = foreign->function->function(FunctionContext(this, stack_index - arguments_count - 1));
        stack_index -= arguments_count + 1;
        push(result);
        return {};
    }

    return RuntimeError("Expected callable value such as function or class.");
}

Upvalue* VM::capture_upvalue(int index) {
    auto* value = &get_from_slot(index);
    // try to reuse already open upvalue
    Upvalue* upvalue = nullptr;
    for (auto open : open_upvalues) {
        if (open->location == value) {
            upvalue = open;
            break;
        }
    }
    if (upvalue == nullptr) {
        upvalue = new Upvalue(value);
        open_upvalues.push_back(upvalue);
        allocate(upvalue);
    }
    return upvalue;
}

void VM::close_upvalues(const Value& value) {
    std::erase_if(
        open_upvalues,
        [&value](Upvalue* open) {
            if (open->location >= &value) {
                // maybe don't use memory addresses for this? seems unsafe
                open->closed = *open->location;
                open->location = &open->closed;
                return true;
            }
            return false;
        }
    );
}

void VM::mark_roots_for_gc() {
    for (int i = 0; i < stack_index; ++i) {
        gc->mark(stack[i]);
    }

    for (auto& frame : frames) {
        gc->mark(frame.closure);
    }

    for (auto* open_upvalue : open_upvalues) {
        gc->mark(open_upvalue);
    }
}

void VM::adopt_objects(std::vector<Object*> objects) {
    for (auto* object : objects) {
        allocate(object);
    }
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

std::optional<Receiver*> VM::get_current_receiver() const {
    Value first_on_stack = stack[frames.back().frame_pointer];
    if (auto object = first_on_stack.as<Object*>()) {
        if (auto* receiver = dynamic_cast<Receiver*>(*object)) {
            return receiver;
        }
    }
    return {};
}


std::optional<VM::RuntimeError> VM::validate_instance_access(Instance* accessor, const ClassValue& class_value) const {
    std::optional<Receiver*> receiver = get_current_receiver();
    if (class_value.attributes[ClassAttributes::PRIVATE] && (!receiver || (*receiver)->instance != accessor)) {
        return RuntimeError("Private propererties can be access only inside their class definitions.");
    }
    return {};
}

std::optional<VM::RuntimeError> VM::validate_class_access(Class* accessor, const ClassValue& class_value) const {
    std::optional<Receiver*> receiver = get_current_receiver();
    if (class_value.attributes[ClassAttributes::PRIVATE] && (!receiver || (*receiver)->klass != accessor)) {
        return RuntimeError("Private propererties can be access only inside their class definitions.");
    }
    return {};
}


Value VM::bind_method(const Value& method, Class* klass, Instance* instance) {
    auto* closure = dynamic_cast<Closure*>(method.get<Object*>());
    auto* receiver = new Receiver(klass, instance);
    auto* bound = new BoundMethod(receiver, closure);
    return bound;
}


std::expected<Value, VM::RuntimeError> VM::get_value_or_bound_method(
    Instance* instance,
    const ClassValue& value,
    bool& is_computed_property
) {
    if (!value.attributes[ClassAttributes::GETTER]) {
        return std::unexpected(RuntimeError("Getter not defined for property."));
    }
    if (value.is_computed) {
        // TODO validate!
        is_computed_property = true;
        auto* property = dynamic_cast<ComputedProperty*>(value.value.get<Object*>());
        return bind_method(property->get.value.value, property->get.owner, instance);
    }
    return value.value;
}

std::variant<std::monostate, Value, VM::RuntimeError> VM::set_value_or_get_bound_method(
    Instance* instance,
    ClassValue& property,
    const Value& value
) {
    if (!property.attributes[ClassAttributes::GETTER]) {
        return RuntimeError("Setter not defined for property.");
    }
    if (property.is_computed) {
        // TODO validate!
        auto* computed = dynamic_cast<ComputedProperty*>(property.value.get<Object*>());
        return bind_method(computed->set.value.value, computed->set.owner, instance);
    }
    property.value = value;
    return {};
}

std::expected<Value, VM::RuntimeError> VM::get_instance_property(
    Instance* instance,
    const std::string& name,
    bool& is_computed_property
) {
    // private or static members get resolved first
    std::optional<Receiver*> receiver = get_current_receiver();
    if (receiver) {
        // try receiver private method
        if (auto class_method = receiver.value()->klass->resolve_private_method(name)) {
            return bind_method(class_method.value().value.value, class_method.value().owner, instance);
        }
        if (auto super_instnace = receiver.value()->instance->get_super_instance_by_class(receiver.value()->klass)) {
            // private property
            if (auto class_property = super_instnace.value()->resolve_private_property(name)) {
                return get_value_or_bound_method(instance, class_property->get(), is_computed_property);
            }
        }
    }
    // members properties
    if (auto class_property = instance->resolve_dynamic_property(name, ClassAttributes::GETTER)) {
        if (std::optional<RuntimeError> error = validate_instance_access(instance, *class_property)) {
            return std::unexpected(*error);
        }
        return get_value_or_bound_method(instance, class_property->get(), is_computed_property);
    }

    if (auto class_method = instance->klass->resolve_dynamic_method(name)) {
        if (std::optional<RuntimeError> error = validate_instance_access(instance, class_method->value)) {
            return std::unexpected(*error);
        }
        return bind_method(class_method->value.value, class_method->owner, instance);
    }

    return std::unexpected(RuntimeError("Property must be declared on class."));
}

std::expected<Value, VM::RuntimeError> VM::get_super_property(
    Instance* super_instance,
    Instance* accessor,
    const std::string& name,
    bool& is_computed_property
) {
    if (auto super_property = super_instance->resolve_dynamic_property(name, ClassAttributes::GETTER)) {
        if (std::optional<RuntimeError> error = validate_instance_access(accessor, super_property.value())) {
            return std::unexpected(*error);
        }
        return get_value_or_bound_method(accessor, super_property.value(), is_computed_property);
    }

    if (auto super_method = super_instance->klass->resolve_dynamic_method(name)) {
        if (std::optional<RuntimeError> error = validate_instance_access(accessor, super_method->value)) {
            return std::unexpected(*error);
        }
        return bind_method(super_method->value.value, super_method->owner, accessor);
    }

    return std::unexpected(RuntimeError("Property must be declared on class."));
}

std::variant<std::monostate, VM::RuntimeError, Value> VM::set_instance_property(
    Instance* instance,
    const std::string& name,
    const Value& value
) {
    std::optional<Receiver*> receiver = get_current_receiver();
    if (receiver) {
        if (auto super_instnace = receiver.value()->instance->get_super_instance_by_class(receiver.value()->klass)) {
            // private property
            if (auto class_property = super_instnace.value()->resolve_private_property(name)) {
                auto bound = set_value_or_get_bound_method(instance, class_property.value(), value);
                if (std::holds_alternative<RuntimeError>(bound)) {
                    return std::get<RuntimeError>(bound);
                }
                if (std::holds_alternative<Value>(bound)) {
                    return std::get<Value>(bound);
                }
                return {};
            }
        }
    }
    // members properties
    if (auto class_property = instance->resolve_dynamic_property(name, ClassAttributes::SETTER)) {
        if (std::optional<RuntimeError> error = validate_instance_access(instance, *class_property)) {
            return *error;
        }
        auto bound = set_value_or_get_bound_method(instance, class_property.value(), value);
        if (std::holds_alternative<RuntimeError>(bound)) {
            return std::get<RuntimeError>(bound);
        }
        if (std::holds_alternative<Value>(bound)) {
            return std::get<Value>(bound);
        }
        return {};
    }

    return RuntimeError("Property must be declared on class.");
}

std::variant<std::monostate, VM::RuntimeError, Value> VM::set_super_property(
    Instance* super_instance,
    Instance* accessor,
    const std::string& name,
    const Value& value
) {
    if (auto super_property = super_instance->resolve_dynamic_property(name, ClassAttributes::SETTER)) {
        if (std::optional<RuntimeError> error = validate_instance_access(accessor, super_property.value())) {
            return *error;
        }
        auto bound = set_value_or_get_bound_method(accessor, super_property.value(), value);
        if (std::holds_alternative<RuntimeError>(bound)) {
            return std::get<RuntimeError>(bound);
        }
        if (std::holds_alternative<Value>(bound)) {
            return std::get<Value>(bound);
        }
        return {};
    }
    return RuntimeError("Property must be declared on class.");
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
                if (frames.empty())
                    return pop();
                break;
            }
            case OpCode::CLOSURE: {
                Function* function = reinterpret_cast<Function*>(*get_constant(fetch()).as<Object*>());
                auto* closure = new Closure(function);
                for (int i = 0; i < closure->get_function()->get_upvalue_count(); ++i) {
                    int is_local = fetch();
                    int index = fetch();
                    if (is_local) {
                        closure->upvalues.push_back(capture_upvalue(index));
                    } else {
                        closure->upvalues.push_back(frames.back().closure->upvalues[index]);
                    }
                }
                // TODO: temp
                if (auto receiver = get_current_receiver()) {
                    auto bound = bind_method(closure, receiver.value()->klass, receiver.value()->instance);
                    push(bound);
                    allocate({bound.get<Object*>(), closure, reinterpret_cast<BoundMethod*>(bound.get<Object*>())->receiver.get<Object*>()});
                } else {
                    push(closure);
                    allocate(closure);
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
            case OpCode::CLASS: {
                int constant_idx = fetch();
                auto name = get_constant(constant_idx).get<std::string>();
                auto* klass = new Class(name);
                if (!peek().is<Nil>()) {
                    klass->class_object = dynamic_cast<Instance*>(peek().get<Object*>());
                }
                pop(); // pop class object
                push(klass);
                allocate(klass);
                break;
            }
            case OpCode::ABSTRACT_CLASS: {
                // overlap?
                int constant_idx = fetch();
                auto name = get_constant(constant_idx).get<std::string>();
                auto* klass = new Class(name);
                if (!peek().is<Nil>()) {
                    klass->class_object = dynamic_cast<Instance*>(peek().get<Object*>());
                }
                pop();
                klass->is_abstract = true;
                push(klass);
                allocate(klass);
                break;
            }
            case OpCode::GET_PROPERTY: {
                std::optional<Object*> object = peek().as<Object*>();
                int constant_idx = fetch();
                auto name = get_constant(constant_idx).get<std::string>();
                if (!object) {
                    return std::unexpected(RuntimeError("Expected class instance value."));
                }
                Instance* instance;
                if (auto* klass = dynamic_cast<Class*>(*object)) {
                    instance = klass->class_object;
                } else {
                    instance = dynamic_cast<Instance*>(*object);
                }

                if (instance) {
                    bool is_computed_property = false;
                    pop();
                    std::expected<Value, RuntimeError> property = get_instance_property(
                        instance,
                        name,
                        is_computed_property
                    );
                    if (!property) {
                        return std::unexpected(property.error());
                    }
                    push(*property);
                    // this will wait for gc refactoring
                    if (property->is<Object*>()) {
                        if (auto* bound = dynamic_cast<BoundMethod*>(property->get<Object*>())) {
                            allocate({ bound, bound->receiver.get<Object*>() });
                            if (is_computed_property) {
                                if (auto error = call_value(bound, 0)) {
                                    return std::unexpected(*error);
                                }
                            }
                        }
                    }
                } else {
                    return std::unexpected(RuntimeError("Expected class object or instance."));
                }

                break;
            }
            case OpCode::SET_PROPERTY: {
                std::optional<Object*> object = peek(0).as<Object*>();
                int constant_idx = fetch();
                auto name = get_constant(constant_idx).get<std::string>();
                if (!object) {
                    return std::unexpected(RuntimeError("Expected class instance value."));
                }

                Instance* instance;
                if (auto* klass = dynamic_cast<Class*>(*object)) {
                    instance = klass->class_object;
                } else {
                    instance = dynamic_cast<Instance*>(*object);
                }

                if (instance) {
                    Value value = peek(1);

                    auto response = set_instance_property(instance, name, value);
                    if (std::holds_alternative<RuntimeError>(response)) {
                        return std::unexpected(std::get<RuntimeError>(response));
                    }
                    if (std::holds_alternative<Value>(response)) {
                        pop(); // pop instance;
                        auto property = std::get<Value>(response);
                        push(property);
                        // gc rewrite!
                        if (property.is<Object*>()) {
                            if (auto* bound = dynamic_cast<BoundMethod*>(property.get<Object*>())) {
                                allocate({ bound, bound->receiver.get<Object*>() });
                            }
                        }
                        pop(); // pop bound for now
                        pop(); // pop value
                        push(property); // push bound
                        push(value); // push argument
                        if (auto error = call_value(property, 1)) {
                            return std::unexpected(*error);
                        }
                    } else {
                        pop(); // pop instance
                    }
                } else {
                    return std::unexpected(RuntimeError("Expected class object or instance."));
                }
                break;
            }
            case OpCode::METHOD: {
                int constant_idx = fetch();
                auto attributes = bitflags<ClassAttributes>(fetch());
                auto name = get_constant(constant_idx).get<std::string>();
                Value method;
                if (!attributes[ClassAttributes::ABSTRACT]) {
                    method = peek();
                }
                Class* klass = dynamic_cast<Class*>(peek(attributes[ClassAttributes::ABSTRACT] ? 0 : 1).get<Object*>());
                // TODO: refactor!
                if (attributes[ClassAttributes::GETTER]) {
                    if (!klass->fields[name].is_computed) {
                        klass->fields[name].is_computed = true;
                        auto* property = new ComputedProperty {};
                        klass->fields[name].value = property;
                        allocate(property);
                    }
                    auto* computed_property = dynamic_cast<ComputedProperty*>(klass->fields[name].value.get<Object*>());
                    computed_property->get = ClassMethod {
                            .value = { .value = method, .attributes = attributes },
                            .owner = klass
                        };
                    klass->fields[name].attributes += ClassAttributes::GETTER;
                } else if (attributes[ClassAttributes::SETTER]) {
                    if (!klass->fields[name].is_computed) {
                        klass->fields[name].is_computed = true;
                        auto* property = new ComputedProperty {};
                        klass->fields[name].value = property;
                        allocate(property);
                    }
                    klass->fields[name].is_computed = true;
                    auto* computed_property = dynamic_cast<ComputedProperty*>(klass->fields[name].value.get<Object*>());
                    computed_property->set = ClassMethod {
                            .value = { .value = method, .attributes = attributes },
                            .owner = klass
                        };
                    klass->fields[name].attributes += ClassAttributes::SETTER;
                } else {
                    klass->methods[name] = { .value = method, .attributes = attributes };
                }

                if (!attributes[ClassAttributes::ABSTRACT]) {
                    pop();
                }
                break;
            }
            case OpCode::INHERIT: {
                Class* superclass = dynamic_cast<Class*>(peek(0).get<Object*>());
                Class* subclass = dynamic_cast<Class*>(peek(1).get<Object*>());
                // for (auto &value: superclass->methods) {
                //     if (value.second.is_private ||) continue;
                //     subclass->methods.insert(value);
                // }
                // for (auto& value : superclass->fields) {
                //     subclass->fields.insert(value);
                // }
                subclass->superclasses = superclass->superclasses;
                subclass->superclasses.push_back(superclass);
                pop();
                break;
            }
            case OpCode::GET_SUPER: {
                int constant_idx = fetch();
                bool is_computed_property = false;
                auto name = get_constant(constant_idx).get<std::string>();
                Instance* accessor = dynamic_cast<Instance*>(peek().get<Object*>());
                std::expected<Value, RuntimeError> property = get_super_property(
                    accessor->get_super().value(),
                    accessor,
                    name,
                    is_computed_property
                );
                if (!property) {
                    return std::unexpected(property.error());
                }
                pop();
                pop();
                push(*property);
                if (property->is<Object*>()) {
                    if (auto* bound = dynamic_cast<BoundMethod*>(property->get<Object*>())) {
                        allocate({ bound, bound->receiver.get<Object*>() });

                        if (is_computed_property) {
                            if (auto error = call_value(bound, 0)) {
                                return std::unexpected(*error);
                            }
                        }
                    }
                }
                break;
            }
            case OpCode::SET_SUPER: {
                int constant_idx = fetch();
                auto name = get_constant(constant_idx).get<std::string>();
                Instance* accessor = dynamic_cast<Instance*>(peek().get<Object*>());
                Value value = peek(1);
                // TODO: i think this super keyword don't work with multiple inheritance levels!
                auto response = set_super_property(accessor->get_super().value(), accessor, name, value);
                if (std::holds_alternative<RuntimeError>(response)) {
                    return std::unexpected(std::get<RuntimeError>(response));
                }
                if (std::holds_alternative<Value>(response)) {
                    pop(); // pop instance;
                    auto property = std::get<Value>(response);
                    push(property);
                    // gc rewrite!
                    if (property.is<Object*>()) {
                        if (auto* bound = dynamic_cast<BoundMethod*>(property.get<Object*>())) {
                            allocate({ bound, bound->receiver.get<Object*>() });
                        }
                    }
                    pop(); // pop bound for now
                    pop(); // pop value
                    push(property); // push bound
                    push(value); // push argument
                    if (auto error = call_value(property, 1)) {
                        return std::unexpected(*error);
                    }
                } else {
                    pop(); // pop class
                }
                break;
            }
            case OpCode::FIELD: {
                int constant_idx = fetch();
                auto attributes = bitflags<ClassAttributes>(fetch());
                auto name = get_constant(constant_idx).get<std::string>();
                Value value;
                Class* klass = dynamic_cast<Class*>(peek().get<Object*>());
                klass->fields[name] = { .value = value, .attributes = attributes };
                break;
            }
            case OpCode::THIS: {
                Receiver* receiver = *get_current_receiver();
                if (receiver->instance != nullptr) {
                    push(receiver->instance);
                } else {
                    push(receiver->klass);
                }
                break;
            }
            case OpCode::CONSTRUCTOR: {
                Value method = peek();
                Class* klass = dynamic_cast<Class*>(peek(1).get<Object*>());
                klass->constructor = method;
                pop();
                break;
            }
            case OpCode::CALL_SUPER_CONSTRUCTOR: {
                // refactor: overlap with call method
                int arguments_count = fetch();
                // should this do any runtime validation?
                Receiver* receiver = *get_current_receiver();
                Class* superclass = receiver->klass->get_super();

                std::vector<Value> args;
                for (int i = 0; i < arguments_count; ++i) {
                    args.push_back(pop());
                }
                //pop(); // pop class
                auto* instance = new Instance(superclass);
                push(instance);
                allocate(instance);

                //receiver->instance->super_instances = instance->super_instances;
                receiver->instance->super_instances.push_back(instance);

                auto bound = bind_method(superclass->constructor, superclass, receiver->instance);
                push(bound);
                // TODO: temp fix
                // work around gc and fix duplicate function returns
                if (auto* bound_ptr = dynamic_cast<BoundMethod*>(bound.get<Object*>())) {
                    allocate({ bound_ptr, bound_ptr->receiver.get<Object*>() });
                }

                std::swap(stack[stack_index - 1], stack[stack_index - 2]);
                pop();

                for (int i = arguments_count - 1; i >= 0; --i) {
                    push(args[i]);
                }
                // TODO: refactor when gc is refactored!

                if (auto res = call_value(bound, arguments_count)) {
                    return std::unexpected(*res);
                }
                break;
            }
            case OpCode::TRAIT: {
                // TODO: trait objects?
                int constant_idx = fetch();
                auto name = get_constant(constant_idx).get<std::string>();
                auto* trait = new Trait(name);
                push(trait);
                allocate(trait);
                break;
            }

            case OpCode::TRAIT_METHOD: {
                int constant_idx = fetch();
                auto name = get_constant(constant_idx).get<std::string>();
                bitflags<ClassAttributes> attributes(fetch());
                // TODO: abstract getters and setters
                // TODO: too much duplication
                if (attributes[ClassAttributes::ABSTRACT]) {
                    Trait* trait = dynamic_cast<Trait*>(peek().get<Object*>());
                    trait->requirements.push_back(name);
                } else {
                    Trait* trait = dynamic_cast<Trait*>(peek(1).get<Object*>());
                    // mess!
                    if (attributes[ClassAttributes::GETTER]) {
                        if (!trait->fields[name].is_computed) {
                            trait->fields[name].is_computed = true;
                            auto* property = new ComputedProperty {};
                            trait->fields[name].value = property;
                            allocate(property);
                        }
                        auto* computed_property = dynamic_cast<ComputedProperty*>(trait->fields[name].value.get<Object
                            *>());
                        computed_property->get = ClassMethod {
                                .value = { .value = peek(), .attributes = attributes },
                                .owner = nullptr
                            }; //?
                        trait->fields[name].attributes += ClassAttributes::GETTER;
                    } else if (attributes[ClassAttributes::SETTER]) {
                        if (!trait->fields[name].is_computed) {
                            trait->fields[name].is_computed = true;
                            auto* property = new ComputedProperty {};
                            trait->fields[name].value = property;
                            allocate(property);
                        }
                        auto* computed_property = dynamic_cast<ComputedProperty*>(trait->fields[name].value.get<Object
                            *>());
                        computed_property->set = ClassMethod {
                                .value = { .value = peek(), .attributes = attributes },
                                .owner = nullptr
                            }; //?
                        trait->fields[name].attributes += ClassAttributes::SETTER;
                    } else {
                        trait->methods[name] = ClassValue {
                                .value = peek(),
                                .attributes = attributes,
                                .is_computed = false
                            };
                    }
                    pop();
                }
                break;
            }
            case OpCode::GET_TRAIT: {
                int constant_idx = fetch();
                bitflags<ClassAttributes> attributes(fetch()); // hacky!
                auto name = get_constant(constant_idx).get<std::string>();
                Trait* trait = dynamic_cast<Trait*>(pop().get<Object*>());
                // TODO: performance
                if (trait->methods.contains(name)) {
                    push(trait->methods[name].value);
                }

                if (trait->fields.contains(name)) {
                    if (attributes[ClassAttributes::GETTER]) {
                        auto* computed_property = dynamic_cast<ComputedProperty*>(trait->fields[name].value.get<Object
                            *>());
                        push(computed_property->get.value.value);
                    } else {
                        auto* computed_property = dynamic_cast<ComputedProperty*>(trait->fields[name].value.get<Object
                            *>());
                        push(computed_property->set.value.value);
                    }
                }
                break;
            }
            case OpCode::GET_GLOBAL: {
                int constant_idx = fetch();
                auto name = get_constant(constant_idx).get<std::string>();
                push(globals[name]); // TODO: what should happen if not present?
                break;
            }
            case OpCode::SET_GLOBAL: {
                int constant_idx = fetch();
                auto name = get_constant(constant_idx).get<std::string>();
                globals[name] = peek();
                break;
            }
            case OpCode::IMPORT: {
                auto module_name = get_constant(fetch()).get<std::string>();
                auto import_name = get_constant(fetch()).get<std::string>();
                auto imported_name = get_constant(fetch()).get<std::string>();
                auto module_name_interned = context->intern(module_name);
                auto import_name_interned = context->intern(import_name);
                BITE_ASSERT(context->get_module(module_name_interned) != nullptr);
                //BITE_ASSERT(context->get_module(module_name_interned)->declarations.contains(item_name_interned));
                auto res = context->get_value_from_module(module_name_interned, import_name_interned);

                // TODO: what happens to colision!
                for (auto& [value_name, value] : res) {
                    globals[imported_name + value_name->substr(import_name.size())] = value;
                }
                // if (auto* value = std::get_if<Value>(&res)) {
                //     push(*value);
                // } else if (auto* func = std::get_if<ForeignFunction*>(&res)) {
                //     push(allocate(
                //         new ForeginFunctionObject(*func)
                //     ));
                // }
                break;
            }
            case OpCode::CLASS_CLOSURE: {
                Function* function = reinterpret_cast<Function*>(*get_constant(fetch()).as<Object*>());
                auto* closure = new Closure(function);
                for (int i = 0; i < closure->get_function()->get_upvalue_count(); ++i) {
                    int is_local = fetch();
                    int index = fetch();
                    if (is_local) {
                        closure->upvalues.push_back(capture_upvalue(index));
                    } else {
                        closure->upvalues.push_back(frames.back().closure->upvalues[index]);
                    }
                }
                push(closure);
                allocate(closure);
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

Object* VM::allocate(Object* ptr) {
    gc->add_object(ptr);
    gc->mark(ptr);
    #ifdef DEBUG_STRESS_GC
    context->run_gc();
    #endif
    if (gc->get_memory_used() > next_gc) {
        mark_roots_for_gc();
        gc->collect();
        next_gc = gc->get_memory_used() * HEAP_GROWTH_FACTOR;
    }

    return ptr;
}

void VM::allocate(std::vector<Object*> ptr) {
    for (auto* p : ptr) {
        gc->add_object(p);
    }
    #ifdef DEBUG_STRESS_GC
    context->run_gc();
    #endif
    if (gc->get_memory_used() > next_gc) {
        mark_roots_for_gc();
        gc->collect();
        next_gc = gc->get_memory_used() * HEAP_GROWTH_FACTOR;
    }
}