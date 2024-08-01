#ifndef FUNCTION_H
#define FUNCTION_H

#include <cassert>
#include <functional>
#include <unordered_map>
#include <utility>
#include <ranges>

#include "Ast.h"
#include "GarbageCollector.h"
#include "Program.h"


class Object {
public:
    bool is_marked = false;

    virtual std::size_t get_size() = 0;

    virtual std::string to_string() = 0;

    virtual void mark_references(GarbageCollector& /*unused*/) {}

    virtual ~Object() = default;
};

class Function final : public Object {
public:
    Function(std::string name, const int arity) : name(std::move(name)),
                                                  arity(arity) {}

    Program& get_program() { return program; }
    [[nodiscard]] int get_arity() const { return arity; }

    int add_constant(const Value& value);

    Value get_constant(int idx);

    [[nodiscard]] int get_upvalue_count() const { return upvalue_count; }
    void set_upvalue_count(const int count) { upvalue_count = count; }

    int add_empty_jump_destination() {
        jump_table.push_back(0);
        return jump_table.size() - 1;
    }

    void patch_jump_destination(int idx, uint32_t destination) {
        assert(idx < jump_table.size());
        jump_table[idx] = destination;
    }

    int add_jump_destination(uint32_t destination) {
        jump_table.push_back(destination);
        return jump_table.size() - 1;
    }

    uint32_t get_jump_destination(int idx) {
        assert(idx < jump_table.size());
        return jump_table[idx];
    }

    std::vector<Value>& get_constants();

    void add_allocated(Object* object);

    const std::vector<Object*>& get_allocated();

    std::size_t get_size() override {
        return sizeof(Function);
    }

    std::string get_name() {
        return name;
    }

    std::string to_string() override {
        return std::format("<Function({})>", name);
    }

    void mark_references(GarbageCollector& gc) override {
        for (auto& constant : constants) {
            gc.mark(constant);
        }
    }

private:
    // objects that were allocated during function compilation
    // vm should adpot them
    std::vector<Object*> allocated_objects;
    std::string name;
    int arity;
    Program program; // code of function
    std::vector<Value> constants;
    std::vector<uint32_t> jump_table;
    int upvalue_count{0};
};

class NativeFunction final : public Object {
public:
    explicit NativeFunction(std::function<Value(const std::vector<Value>&)> function) : function(std::move(function)) {}

    std::size_t get_size() override {
        return sizeof(NativeFunction);
    }

    std::string to_string() override {
        return "<Native>";
    }

    std::function<Value(const std::vector<Value>&)> function;
};

class Upvalue : public Object {
public:
    explicit Upvalue(Value* location) : location(location) {}

    std::size_t get_size() override {
        return sizeof(Upvalue);
    }

    std::string to_string() override {
        return std::format("<Upvalue({})>", location->to_string());
    }

    void mark_references(GarbageCollector& gc) override {
        gc.mark(closed);
    }

    Value* location = nullptr;
    Value closed = Value { nil_t };
};

class Closure final : public Object {
public:
    explicit Closure(Function* function) : function(function) {}

    [[nodiscard]] Function* get_function() const { return function; }

    std::size_t get_size() override {
        return sizeof(Closure);
    }

    std::string to_string() override {
        return std::format("<Closure({})>", function->to_string());
    };

    void mark_references(GarbageCollector& gc) override {
        for (auto* upvalue : upvalues) {
            gc.mark(upvalue);
        }
        gc.mark(function);
    }

    std::vector<Upvalue*> upvalues;

private:
    Function* function;
};

struct ClassValue {
    Value value;
    bitflags<ClassAttributes> attributes;
    bool is_computed = false; // if is computed don't look at this attributes?
};

class Class;

struct ClassMethod {
    ClassValue value;
    Class* owner = nullptr;
};


class ComputedProperty : public Object {
public:
    std::size_t get_size() override {
        return sizeof(ComputedProperty);
    }

    std::string to_string() override {
        return std::format("<ComputedPropety()>"); // TODO: implement
    }

    void mark_references(GarbageCollector& gc) override {
        gc.mark(get.value.value);
        gc.mark(set.value.value);
    }

    ClassMethod get;
    ClassMethod set;
};

class Instance;

class Class final : public Object {
public:
    explicit Class(std::string name) : name(std::move(name)) {}

    std::size_t get_size() override {
        return sizeof(Class);
    }

    std::string to_string() override {
        return std::format("<Class({})>", name);
    }

    void mark_references(GarbageCollector& gc) override;

    // resolve only current class memebers
    std::optional<ClassMethod> resolve_private_method(const std::string& name) {
        if (!methods.contains(name) || !methods[name].attributes[ClassAttributes::PRIVATE]) {
            return {};
        }
        return { { methods[name], this } };
    }

    std::optional<ClassMethod> resolve_dynamic_method(const std::string& name) {
        if (methods.contains(name) && !methods[name].attributes[ClassAttributes::PRIVATE]) {
            return { { methods[name], this } };
        }
        for (auto* superclass : std::views::reverse(superclasses)) {
            if (superclass->methods.contains(name) && !superclass->methods[name].attributes[ClassAttributes::PRIVATE]) {
                return { { superclass->methods[name], this } };
            }
        }
        return {};
    }

    Class* get_super() {
        // check?
        return superclasses.back();
    }

    std::string name;
    std::unordered_map<std::string, ClassValue> methods;
    std::unordered_map<std::string, ClassValue> fields;
    std::vector<Class*> superclasses;
    Instance* class_object = nullptr;
    Value constructor;
    bool is_abstract = false;
};


class Instance final : public Object {
public:
    explicit Instance(Class* klass) : klass(klass) {
        // default intialize properties
        // should this be here or in vm?
        for (auto& [name, value] : klass->fields) {
            properties[name] = value;
        }
    }

    std::size_t get_size() override {
        return sizeof(Instance);
    }

    std::string to_string() override {
        return std::format("<Instance({})>", klass->to_string());
    }

    void mark_references(GarbageCollector& gc) override {
        gc.mark(klass);
        for (auto& value : std::views::values(properties)) {
            gc.mark(value.value);
        }
        for (auto* super_instance : super_instances) {
            gc.mark(super_instance);
        }
    }

    std::optional<std::reference_wrapper<ClassValue>> resolve_private_property(const std::string& name) {
        if (properties.contains(name) && properties[name].attributes[ClassAttributes::PRIVATE]) {
            return properties[name];
        }
        return {};
    }

    std::optional<std::reference_wrapper<ClassValue>> resolve_dynamic_property(
        const std::string& name,
        std::optional<ClassAttributes> attribute_to_match = {}
    ) {
        if (properties.contains(name) && !properties[name].attributes[ClassAttributes::PRIVATE] && (!attribute_to_match
            || properties[name].attributes[*attribute_to_match])) {
            return properties[name];
        }
        for (auto* super_instance : super_instances) {
            if (super_instance->properties.contains(name) && !super_instance->properties[name].attributes[
                ClassAttributes::PRIVATE] && (!attribute_to_match || super_instance->properties[name].attributes[*
                attribute_to_match])) {
                return super_instance->properties[name];
            }
        }
        return {};
    }

    std::optional<Instance*> get_super_instance_by_class(Class* klass) {
        for (auto* super_instance : super_instances) {
            if (super_instance->klass == klass) {
                return super_instance;
            }
        }
        return {};
    }

    std::optional<Instance*> get_super() {
        if (super_instances.empty()) {
            return {};
        }

        return super_instances[0];
    }

    Class* klass;
    std::vector<Instance*> super_instances;
    std::unordered_map<std::string, ClassValue> properties;
};

// TODO: investigate simpler runtime representation of traits maybe as classes?
class Trait : public Object {
public:
    explicit Trait(std::string name) : name(std::move(name)) {}

    std::size_t get_size() override {
        return sizeof(Trait);
    }

    std::string to_string() override {
        return { "<trait>" }; // TODO: implement
    }

    void mark_references(GarbageCollector& gc) override {
        for (auto& method : methods | std::views::values) {
            gc.mark(method.value);
        }
        for (auto& method : fields | std::views::values) {
            gc.mark(method.value);
        }
    }

    std::string name;
    std::unordered_map<std::string, ClassValue> methods;
    std::unordered_map<std::string, ClassValue> fields;
    std::vector<std::string> requirements;
};

/**
 * klass = class context where method is dispatched
 */
class Receiver final : public Object {
public:
    Receiver(Class* klass, Instance* instance) : klass(klass),
                                                 instance(instance) {}

    std::size_t get_size() override {
        return sizeof(Receiver);
    }

    std::string to_string() override {
        return std::format("<Receiver({}, {})>", klass->to_string(), instance->to_string());
    }

    void mark_references(GarbageCollector& gc) override {
        gc.mark(klass);
        gc.mark(instance);
    }

    Class* klass;
    Instance* instance;
};

class BoundMethod final : public Object {
public:
    BoundMethod(Value receiver, Closure* closure) : receiver(std::move(receiver)),
                                                    closure(closure) {}

    std::size_t get_size() override {
        return sizeof(BoundMethod);
    }

    std::string to_string() override {
        return std::format("<BoundMethod({}, {})>", receiver.to_string(), closure->to_string());
    }

    void mark_references(GarbageCollector& gc) override {
        gc.mark(receiver);
        gc.mark(closure);
    }

    Value receiver;
    Closure* closure;
};

#endif //FUNCTION_H
