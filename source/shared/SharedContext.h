#ifndef CONTEXT_H
#define CONTEXT_H
#include "StringTable.h"
#include "../base/logger.h"
#include "../parser/Lexer.h"


/**
 * Shared context between compilation stages
 */
class SharedContext {
public:
    explicit SharedContext(bite::Logger logger) : logger(std::move(logger)) {}

    StringTable::Handle intern(const std::string& string) {
        return string_table.intern(string);
    }

    bite::Logger logger;
private:
    StringTable string_table;
};
#endif //CONTEXT_H
