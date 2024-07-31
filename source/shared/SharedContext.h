#ifndef CONTEXT_H
#define CONTEXT_H
#include "Message.h"
#include "StringTable.h"
#include "../base/logger.h"
#include "../parser/CompilationMessage.h"
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

    void add_compilation_message(bite::Message message) {
        messages.push_back(std::move(message));
    }

    const std::vector<bite::Message>& get_compilation_messages() {
        return messages;
    }

    bite::Logger logger;
private:
    StringTable string_table;
    std::vector<bite::Message > messages;
};
#endif //CONTEXT_H
