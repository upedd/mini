//
// Created by MiłoszK on 22.06.2024.
//

#ifndef MINICALLABLE_H
#define MINICALLABLE_H
#include <any>
#include <vector>
#include "Interpreter.h"

class MiniCallable {
public:
    virtual std::any call(Interpreter* interpreter, std::vector<std::any> arguments) = 0;
    virtual int arity() = 0;
    virtual ~MiniCallable() = default;
};

#endif //MINICALLABLE_H
