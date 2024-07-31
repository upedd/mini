#ifndef MESSAGE_H
#define MESSAGE_H
#include <optional>
#include <string>

#include "../base/logger.h"

namespace bite {
    struct InlineMessage {
        std::size_t source_start;
        std::size_t source_end;
        std::string Message;
    };

    struct Message {
        Logger::Level level;
        std::string content;
        std::optional<InlineMessage> inline_msg;
    };
}


#if __has_include(<format>)

#endif


#endif //MESSAGE_H
