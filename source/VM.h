#ifndef VM_H
#define VM_H
#include <stdexcept>
#include <vector>

#include "CallFrame.h"
#include "Object.h"

#define DEBUG_STRESS_GC

class VM {
public:
    class RuntimeError : public std::runtime_error {
    public:
        explicit RuntimeError(const std::string& message) : std::runtime_error(message) {};
    };

    explicit VM(GarbageCollector gc, Function* function) : gc(std::move(gc)) {
        auto* closure = new Closure(function);
        // todo: maybe start program thru call()
        frames.emplace_back(closure, 0, 0);
        allocate<Closure>(closure);
        // adopt_objects(function->get_allocated());
    }

    uint8_t fetch();
    OpCode fetch_opcode();
    uint16_t fetch_short();

    void jump_to(uint32_t pos);
    [[nodiscard]] Value get_constant(int idx) const;

    uint32_t get_jump_destination(int idx) const;

    Value pop();
    [[nodiscard]] Value peek(int n = 0) const;
    void push(const Value& value);

    Value &get_from_slot(int index);
    void set_in_slot(int index, const Value &value);

    std::optional<RuntimeError> call_value(const Value &value, int arguments_count);

    Upvalue* capture_upvalue(int index);

    void close_upvalues(const Value & peek);

    void mark_roots_for_gc();
    void run_gc();

    template<class T>
    T *allocate(T *ptr);

    void adopt_objects(std::vector<Object*> objects);
    bool bind_method(Class * klass, const std::string & name);

    std::optional<Instance *> get_current_instance();

    std::expected<Value, RuntimeError> run();

    void add_native(const std::string& name, const Value &value);
    void add_native_function(const std::string &name, const std::function<Value(const std::vector<Value> &)> &fn);
private:
    GarbageCollector gc;
    std::size_t next_gc = 1024 * 1024;
    std::vector<int> block_stack;
    static constexpr std::size_t HEAP_GROWTH_FACTOR = 2;
    // stack dynamic allocation breaks upvalues!
    int stack_index = 0;
    std::array<Value, 256> stack;
    std::vector<CallFrame> frames;
    std::list<Upvalue*> open_upvalues;
    std::unordered_map<std::string, Value> natives;
};

template<typename T>
T* VM::allocate(T* ptr) {
    gc.add_object(ptr);
#ifdef DEBUG_STRESS_GC
    run_gc();
#endif
    if (gc.get_memory_used() > next_gc) {
        mark_roots_for_gc();
        gc.collect();
        next_gc = gc.get_memory_used() * HEAP_GROWTH_FACTOR;
    }

    return ptr;
}

#endif //VM_H
