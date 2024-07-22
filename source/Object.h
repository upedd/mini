#ifndef FUNCTION_H
#define FUNCTION_H

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
    virtual void mark_references(GarbageCollector& gc) {}

    virtual ~Object() = default;
};

class Function final : public Object {
public:
    Function(std::string name, const int arity) : name(std::move(name)),
                                                  arity(arity), upvalue_count(0) {}

    Program &get_program() { return program; }
    [[nodiscard]] int get_arity() const { return arity; }

    int add_constant(const Value &value);

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
    const std::vector<Object*> get_allocated();

    std::size_t get_size() override {
        return sizeof(Function);
    };

    std::string to_string() override {
        return std::format("<Function({})>", name);
    }

    void mark_references(GarbageCollector &gc) override {
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
    int upvalue_count;
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

    void mark_references(GarbageCollector &gc) override {
        gc.mark(closed);
    }

    Value* location = nullptr;
    Value closed = Value{nil_t};
};

class Closure final : public Object {
public:
    explicit Closure(Function *function) : function(function) {
    }

    [[nodiscard]] Function *get_function() const { return function; }

    std::size_t get_size() override {
        return sizeof(Closure);
    }

    std::string to_string() override {
        return std::format("<Closure({})>", function->to_string());
    };

    void mark_references(GarbageCollector &gc) override {
        for (auto* upvalue : upvalues) {
            gc.mark(upvalue);
        }
        gc.mark(function);
    }

    std::vector<Upvalue *> upvalues;

private:
    Function *function;
};

struct ClassValue {
    Value value;
    bool is_private = false;
    bool is_static = false;
    bool is_override = false;
};

class Class final : public Object{
public:
    explicit Class(std::string  name) : name(std::move(name)) {}

    std::size_t get_size() override {
        return sizeof(Class);
    }

    std::string to_string() override {
        return std::format("<Class({})>", name);
    }

    void mark_references(GarbageCollector &gc) override {
        for (auto& value : std::views::values(methods)) {
            gc.mark(value.value);
        }
        for (auto& value : std::views::values(fields)) {
            gc.mark(value.value);
        }
        gc.mark(superclass);
    }

    std::string name;
    std::unordered_map<std::string, ClassValue> methods;
    std::unordered_map<std::string, ClassValue> fields;
    Class* superclass = nullptr;
};



class Instance final : public Object {
public:
    explicit Instance(Class* klass) : klass(klass) {
        // default intialize properties
        // should this be here or in vm?
        for (auto& [name, value] : klass->fields) {
            if (!value.is_static) {
                properties[name] = value;
            }
        }
    }

    std::size_t get_size() override {
        return sizeof(Instance);
    }

    std::string to_string() override {
        return std::format("<Instance({})>", klass->to_string());
    }

    void mark_references(GarbageCollector &gc) override {
        gc.mark(klass);
        for (auto& value : std::views::values(properties)) {
            gc.mark(value.value);
        }
        gc.mark(super_instance);
    }

    Class* klass;
    Instance* super_instance = nullptr;
    std::unordered_map<std::string, ClassValue> properties;
};


/**
 * klass = class context where method is dispatched
 */
class Receiver final : public Object {
public:
    Receiver(Class* klass, Instance* instance) : klass(klass), instance(instance) {}

    std::size_t get_size() override {
        return sizeof(Receiver);
    }

    std::string to_string() override {
        return std::format("<Receiver({}, {})>",
            klass->to_string(),
            instance->to_string());
    }

    void mark_references(GarbageCollector &gc) override {
        gc.mark(klass);
        gc.mark(instance);
    }

    Class* klass;
    Instance* instance;
};

class BoundMethod final : public Object {
public:
    BoundMethod(Value receiver, Closure *closure)
        : receiver(std::move(receiver)),
          closure(closure) {
    }

    std::size_t get_size() override {
        return sizeof(BoundMethod);
    }

    std::string to_string() override {
        return std::format("<BoundMethod({}, {})>",
            receiver.to_string(),
            closure->to_string());
    }

    void mark_references(GarbageCollector &gc) override {
        gc.mark(receiver);
        gc.mark(closure);
    }

    Value receiver;
    Closure* closure;
};

#endif //FUNCTION_H
