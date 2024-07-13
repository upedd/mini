#ifndef FUNCTION_H
#define FUNCTION_H

#include <functional>
#include <unordered_map>
#include <utility>
#include <ranges>

#include "Expr.h"
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
    int upvalue_count;
};

class NativeFunction final : public Object {
public:
    explicit NativeFunction(std::function<void()> function) : function(std::move(function)) {}

    std::size_t get_size() override {
        return sizeof(NativeFunction);
    }

    std::string to_string() override {
        return "<Native>";
    }

    std::function<void()> function;
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
            gc.mark(value);
        }
    }

    std::string name;
    std::unordered_map<std::string, Value> methods;
};

class Instance final : public Object {
public:
    explicit Instance(Class* klass) : klass(klass) {}

    std::size_t get_size() override {
        return sizeof(Instance);
    }

    std::string to_string() override {
        return std::format("<Instance({})>", klass->to_string());
    }

    void mark_references(GarbageCollector &gc) override {
        gc.mark(klass);
        for (auto& value : std::views::values(fields)) {
            gc.mark(value);
        }
    }

    Class* klass;
    std::unordered_map<std::string, Value> fields;
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

    Value receiver;
    Closure* closure;
};

#endif //FUNCTION_H
