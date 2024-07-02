#ifndef FUNCTION_H
#define FUNCTION_H

#include "Module.h"

struct Function {
    int arity = 0;
    Module code;
    std::string name;
};

inline Function* allocate_function() {
    auto* ptr = new Function(); // memory!
    return ptr;
}


#endif //FUNCTION_H
