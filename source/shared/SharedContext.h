#ifndef CONTEXT_H
#define CONTEXT_H
#include "StringTable.h"

/**
 * Shared context between compilation stages
 */
class SharedContext {
public:
    StringTable::Handle intern(const std::string& string) {
        return string_table.intern(string);
    }
private:
    StringTable string_table;
};
#endif //CONTEXT_H
