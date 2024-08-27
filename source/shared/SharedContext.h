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


// TODO: temp
class Module {
public:
    virtual ~Module() = default;
};

class ForeignModule final : public Module {
public:
    bite::unordered_dense::segmented_map<StringTable::Handle, ForeignFunction> functions;
};

class FileModule final : public Module {
public:
    bool m_was_executed = false;
    Function* function;
    bite::unordered_dense::map<StringTable::Handle, Declaration*> declarations;
    bite::unordered_dense::map<StringTable::Handle, Value> values;

    FileModule(Function* function, bite::unordered_dense::map<StringTable::Handle, Declaration*> declarations) :
        function(function),
        declarations(std::move(declarations)) {}
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
    FileModule* compile(const std::string& file);
    void execute(FileModule& module);
    void add_module(const StringTable::Handle name, std::unique_ptr<ForeignModule> module);
    std::variant<Value, ForeignFunction*> get_value_from_module(
        const StringTable::Handle& module,
        const StringTable::Handle& name
    );

    void run_gc();

    bite::Logger logger;
    bite::DiagnosticManager diagnostics;
    GarbageCollector gc;
    std::deque<VM> running_vms;

private:
    // need to store them for lifetime reasons
    std::deque<Ast> ast_storage;
    StringTable string_table;
    bite::unordered_dense::segmented_map<StringTable::Handle, std::unique_ptr<Module>> modules;
};
#endif //CONTEXT_H
