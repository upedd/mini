#ifndef VM_H
#define VM_H
#include <optional>
#include <stdexcept>
#include <vector>

#include "Module.h"
#include "ModuleReader.h"


class VM {
public:
    class RuntimeError : public std::runtime_error {
    public:
        explicit RuntimeError(const std::string& message) : std::runtime_error(message) {};
    };

    explicit VM(const Module& module) : reader(module) {}


    void tick();
    void run();
private:
    ModuleReader reader;
    std::vector<Value> stack; // optim: should stack could be fixed array?
};



#endif //VM_H
