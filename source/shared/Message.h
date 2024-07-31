#ifndef MESSAGE_H
#define MESSAGE_H
#include <optional>
#include <string>

#include "../base/logger.h"

namespace bite {
    struct SourceLocation {
        std::string file_path;
        std::size_t start_offset;
        std::size_t end_offset;
    };

    struct InlineMessage {
        SourceLocation location;
        std::string Message;
    };

    struct Message {
        Logger::Level level;
        std::string content;
        std::optional<InlineMessage> inline_msg;
    };
}

#endif //MESSAGE_H
