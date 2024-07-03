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

// struct Function {
//     int arity = 0;
//     Module code;
//     std::string name;
// };
//
// inline Function* allocate_function() {
//     auto* ptr = new Function(); // memory!
//     return ptr;
// }


#endif //FUNCTION_H
