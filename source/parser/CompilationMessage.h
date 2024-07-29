#ifndef COMPILATIONMESSAGE_H
#define COMPILATIONMESSAGE_H
#include <string>

struct CompilationMessage {
    enum class Type {
        ERROR,
        WARRING
    };

    Type type;
    std::string reason;
    std::string inline_message;
    std::size_t source_offset_start = -1;
    std::size_t source_offset_end = -1;
};
#endif //COMPILATIONMESSAGE_H
