#ifndef FUNCTION_H
#define FUNCTION_H

#include <utility>

#include "Expr.h"
#include "Program.h"

class Function {
public:
    Function(std::string name, const int arity) : name(std::move(name)), arity(arity) {}

    Program& get_program() { return program; }
    [[nodiscard]] int get_arity() const { return arity; }

    int add_constant(const Value & value);
    Value get_constant(int idx);

    [[nodiscard]] int get_upvalue_count() const {return upvalue_count;}
    void set_upvalue_count(const int count) {upvalue_count = count;}

private:
    std::string name;
    int arity;
    Program program; // code of function
    std::vector<Value> constants;
    int upvalue_count;
};

class Closure {
public:
    explicit Closure(Function* function) : function(function) {}
    [[nodiscard]] Function* get_function() const { return function; }
private:
    Function* function;
};

#endif //FUNCTION_H
