#ifndef CONTEXT_H
#define CONTEXT_H
#include <fstream>
#include <stack>

#include "StringTable.h"
#include "../Diagnostics.h"
#include "../base/logger.h"
#include "../Ast.h"
#include "../GarbageCollector.h"
#include "../VM.h"

// TODO: find better place

class Function;
class FunctionContext {
public:
    explicit FunctionContext(VM* vm, int64_t frame_pointer) : vm(vm),
                                                              frame_pointer(frame_pointer) {}

    Value get_arg(int64_t pos);

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
    bool m_was_executed = false;
    Function* function;
    bite::unordered_dense::map<StringTable::Handle, Declaration*> declarations;
    bite::unordered_dense::map<StringTable::Handle, Value> values;
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

    Module* get_module(StringTable::Handle name);
    Module* compile(const std::string& file);

    void add_module(const StringTable::Handle& name);;

    void execute(Module& module);

    Value get_value_from_module(const StringTable::Handle& module, const StringTable::Handle& name);

    void run_gc();

    bite::Logger logger;
    bite::DiagnosticManager diagnostics;
    GarbageCollector gc;
    std::deque<VM> running_vms;
private:
    // need to store them for lifetime reasons
    std::deque<Ast> ast_storage;
    StringTable string_table;
    bite::unordered_dense::segmented_map<StringTable::Handle, Module> modules;
};
#endif //CONTEXT_H
