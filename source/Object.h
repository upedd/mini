#ifndef FUNCTION_H
#define FUNCTION_H

#include <unordered_map>
#include <utility>

#include "Expr.h"
#include "Program.h"


class Object {
public:
    bool is_marked = false;
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

class Upvalue : public Object {
public:
    explicit Upvalue(Value* location) : location(location) {}
    Value* location = nullptr;
    Value closed = Value{nil_t};
};

class Closure final : public Object {
public:
    explicit Closure(Function *function) : function(function) {
    }

    [[nodiscard]] Function *get_function() const { return function; }

    std::vector<Upvalue *> upvalues;

private:
    Function *function;
};

class Class final : public Object{
public:
    explicit Class(std::string  name) : name(std::move(name)) {}
    std::string name;
    std::unordered_map<std::string, Value> methods;
};

class Instance final : public Object {
public:
    explicit Instance(Class* klass) : klass(klass) {}

    Class* klass;
    std::unordered_map<std::string, Value> fields;
};

class BoundMethod final : public Object {
public:
    BoundMethod(Value receiver, Closure *closure)
        : receiver(std::move(receiver)),
          closure(closure) {
    }

    Value receiver;
    Closure* closure;
};

#endif //FUNCTION_H
