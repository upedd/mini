#ifndef CONTEXT_H
#define CONTEXT_H
#include "StringTable.h"
#include "../Diagnostics.h"
#include "../base/logger.h"


/**
 * Shared context between compilation stages
 */
class SharedContext {
public:
    explicit SharedContext(const bite::Logger& logger) : logger(logger) {}

    StringTable::Handle intern(const std::string& string) {
        return string_table.intern(string);
    }

    bite::Logger logger;
    bite::DiagnosticManager diagnostics;
private:
    StringTable string_table;
};
#endif //CONTEXT_H
