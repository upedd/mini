#ifndef VM_H
#define VM_H
#include <vector>

#include "Module.h"
#include "ModuleReader.h"


class VM {
public:
    explicit VM(const Module& module) : reader(module) {}

    void tick();
    void run();
private:
    ModuleReader reader;
    std::vector<Value> stack; // optim: should stack could be fixed array?
};



#endif //VM_H
