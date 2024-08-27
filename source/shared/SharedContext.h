#ifndef CONTEXT_H
#define CONTEXT_H
#include "StringTable.h"
#include "../Diagnostics.h"
#include "../base/logger.h"
#include "../Ast.h"
#include "../VM.h"

// TODO: find better place

class VM;

class FunctionContext {
public:
    explicit FunctionContext(VM* vm, int64_t frame_pointer) : vm(vm), frame_pointer(frame_pointer) {}

    Value get_arg(int64_t pos) {
        return vm->stack[frame_pointer + pos + 1];
    }
private:
    VM* vm;
    int64_t frame_pointer;
};


struct ForeignFunction {
    int arity {};
    StringTable::Handle name {};
    std::function<Value(FunctionContext)> function;
};

class Module {
public:
    bite::unordered_dense::segmented_map<StringTable::Handle, ForeignFunction> functions;
};


/**
 * Shared context between compilation stages
 */
class SharedContext {
public:
    explicit SharedContext(const bite::Logger& logger) : logger(logger) {}

    StringTable::Handle intern(const std::string& string) {
        return string_table.intern(string);
    }

    Module* get_module(StringTable::Handle name) {
        if (modules.contains(name)) {
            return &modules[name];
        }
        return nullptr;
    }


    void add_module(const StringTable::Handle& name) {
        modules[name] = Module();
    };

    void add_function(
        const StringTable::Handle& module,
        ForeignFunction function
    ) {
        BITE_ASSERT(modules.contains(module));
        modules[module].functions[function.name] = std::move(function);
    };

    bite::Logger logger;
    bite::DiagnosticManager diagnostics;

private:
    StringTable string_table;
    bite::unordered_dense::segmented_map<StringTable::Handle, Module> modules;
};
#endif //CONTEXT_H
