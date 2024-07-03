#ifndef VM_H
#define VM_H
#include <stdexcept>
#include <vector>

#include "CallFrame.h"

class VM {
public:
    class RuntimeError : public std::runtime_error {
    public:
        explicit RuntimeError(const std::string& message) : std::runtime_error(message) {};
    };

    explicit VM(Function* function) {
        frames.emplace_back(function, 0, 0);
    }

    uint8_t fetch();
    OpCode fetch_opcode();
    uint16_t fetch_short();
    void jump_by_offset(int offset);
    [[nodiscard]] Value get_constant(int idx) const;
    Value pop();
    [[nodiscard]] Value peek(int n = 0) const;
    void push(const Value& value);
    Value get_from_slot(int index);
    void set_in_slot(int index, const Value &value);

    std::optional<RuntimeError> call_value(const Value &value, int arguments_count);

    std::expected<Value, RuntimeError> run();
private:
    std::vector<Value> stack; // optim: should stack could be fixed array?
    std::vector<CallFrame> frames;
};



#endif //VM_H
