#ifndef VM_H
#define VM_H
#include <set>
#include <stdexcept>
#include <vector>

#include "CallFrame.h"

#define DEBUG_STRESS_GC

#define DEBUG_LOG_GC

#ifdef DEBUG_LOG_GC
#include <iostream>
#endif

class VM {
public:
    class RuntimeError : public std::runtime_error {
    public:
        explicit RuntimeError(const std::string& message) : std::runtime_error(message) {};
    };

    explicit VM(Function* function) {
        // todo: maybe start program thru call()
        frames.emplace_back(new Closure(function), 0, 0);
    }

    uint8_t fetch();
    OpCode fetch_opcode();
    uint16_t fetch_short();
    void jump_by_offset(int offset);
    [[nodiscard]] Value get_constant(int idx) const;
    Value pop();
    [[nodiscard]] Value peek(int n = 0) const;
    void push(const Value& value);

    Value &get_from_slot(int index);
    void set_in_slot(int index, const Value &value);

    std::optional<RuntimeError> call_value(const Value &value, int arguments_count);

    Upvalue* capture_upvalue(int index);

    void close_upvalues(const Value & peek);

    // Garbage collection
    template<typename T>
    Object allocate();

    void mark_object(Object* object);

    void mark_value(const Value& value);

    void mark_roots();

    void blacken_object(Object * object);

    void trace_references();

    void collect_garbage();

    std::vector<Object*> gray_objects;

    std::expected<Value, RuntimeError> run();
private:
    // stack dynamic allocation breaks upvalues!
    int stack_index = 0;
    std::array<Value, 256> stack; // optim: should stack could be fixed array?
    std::vector<CallFrame> frames;
    std::set<Upvalue*> open_upvalues;
};

template<typename T>
Object VM::allocate() {
#ifdef DEBUG_STRESS_GC
    collect_garbage();
#endif
    Object ptr = Object(new T());

#ifdef DEBUG_LOG_GC
    std::cout << "Allocated " << sizeof(T) << " bytes of memory.\n";
#endif

    return ptr;
}


#endif //VM_H
