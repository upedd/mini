#ifndef CONTEXT_H
#define CONTEXT_H
#include "StringTable.h"
#include "../parser/CompilationMessage.h"
#include "../parser/Lexer.h"


/**
 * Shared context between compilation stages
 */
class SharedContext {
public:
    StringTable::Handle intern(const std::string& string) {
        return string_table.intern(string);
    }

    void add_compilation_message(CompilationMessage message) {
        messages.push_back(std::move(message));
    }

    const std::vector<CompilationMessage>& get_compilation_messages() {
        return messages;
    }
private:
    StringTable string_table;
    std::vector<CompilationMessage> messages;
};
#endif //CONTEXT_H
