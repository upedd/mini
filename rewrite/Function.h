#ifndef FUNCTION_H
#define FUNCTION_H

#include <utility>

#include "Expr.h"
#include "Module.h"
#include "Program.h"

class Function {
public:
    Function(std::string name, const int arity) : name(std::move(name)), arity(arity) {}

    Program& get_program() { return program; }

    int add_constant(const Value & value);

private:
    std::string name;
    int arity;
    Program program; // code of function
    std::vector<Value> constants;
};

#endif //FUNCTION_H
