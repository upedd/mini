#ifndef VM_H
#define VM_H
#include <stdexcept>
#include <vector>

#include "CallFrame.h"
#include "Module.h"
#include "ModuleReader.h"

class VM {
public:
    class RuntimeError : public std::runtime_error {
    public:
        explicit RuntimeError(const std::string& message) : std::runtime_error(message) {};
    };

    explicit VM(Function* function) {
        frames.emplace_back(function, 0, 0);
        reader.set_frame(frames.back());
    }

    void call_value(Value& value, int arguments_count);

    void tick();
    void run();
private:
    ModuleReader reader;
    std::vector<Value> stack; // optim: should stack could be fixed array?
    std::vector<CallFrame> frames;
};



#endif //VM_H
