#ifndef CONTEXT_H
#define CONTEXT_H
#include "StringTable.h"
#include "../Diagnostics.h"
#include "../base/logger.h"


// TODO: find better place
class Module {
public:
    bite::unordered_dense::map<StringTable::Handle, Declaration*> items;
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

    bite::Logger logger;
    bite::DiagnosticManager diagnostics;
private:
    StringTable string_table;
    bite::unordered_dense::segmented_map<StringTable::Handle, Module> modules;
};
#endif //CONTEXT_H
