#ifndef CONTEXT_H
#define CONTEXT_H
#include "StringTable.h"
#include "../parser/Lexer.h"


/**
 * Shared context between compilation stages
 */
class SharedContext {
public:
    StringTable::Handle intern(const std::string& string) {
        return string_table.intern(string);
    }

    void add_error(std::size_t start_offset, std::size_t end_offset, const std::string& message) {
        errors.emplace_back(start_offset, message);
    }
    const std::vector<Lexer::Error>& get_errors() {
        return errors;
    }
private:
    StringTable string_table;
    std::vector<Lexer::Error> errors;
};
#endif //CONTEXT_H
