#ifndef ENHANCE_MESSAGES_H
#define ENHANCE_MESSAGES_H
#include <filesystem>

#include "../shared/Message.h"

namespace bite {
    struct EnchancedInfo {
        std::string line;
        std::size_t line_number;
        std::size_t in_line_start;
    };

    struct EnchancedMessage {
        Message message;
        std::optional<EnchancedInfo> info;
    };

    // TODO: refactor:
    namespace detail {
        inline void enchance_messages_from_source(
            const std::vector<Message>& messages,
            std::vector<EnchancedMessage>& output,
            std::vector<bool>& processed,
            const std::string& path,
            const std::vector<std::size_t>& messages_to_process
        ) {
            std::ifstream file(path);
            std::size_t line_counter = 0;
            std::string current_line;
            std::size_t previous_line_offset = 0;
            while (std::getline(file, current_line)) {
                ++line_counter;
                std::size_t current_line_offset = previous_line_offset + current_line.size() + 1; // why plus one here?
                for (auto idx : messages_to_process) {
                    if (processed[idx] == true)
                        continue;
                    std::size_t start_offset = messages[idx].inline_msg->location.start_offset;
                    if (start_offset < current_line_offset) {
                        processed[idx] = true;
                        output.emplace_back(
                            messages[idx],
                            EnchancedInfo {
                                .line = current_line,
                                .line_number = line_counter,
                                .in_line_start = start_offset - previous_line_offset
                            }
                        );
                    }
                }
                previous_line_offset = current_line_offset;
            }
        }
    }

    inline std::vector<EnchancedMessage> enchance_messages(const std::vector<Message>& messages) {
        std::vector<bool> processed(messages.size());
        std::vector<EnchancedMessage> output(messages.size());
        for (std::size_t i = 0; i < messages.size(); ++i) {
            if (processed[i])
                continue;
            auto& message = messages[i];
            if (!message.inline_msg) {
                // Cannot enchance messages without inline messages
                output[i] = { message };
            } else {
                // Proccess file
                std::vector<std::size_t> same_source_messages;
                same_source_messages.push_back(i);
                auto& message_path = message.inline_msg->location.file_path;
                for (std::size_t j = i + 1; j < messages.size(); ++j) {
                    if (messages[j].inline_msg && messages[j].inline_msg->location.file_path == message_path) {
                        same_source_messages.push_back(j);
                    }
                }
                detail::enchance_messages_from_source(messages, output, processed, message_path, same_source_messages);
            }
        }
        return output;
    }


    template <>
    struct LogPrinter<EnchancedMessage> {
        void operator()(const EnchancedMessage& enchanced, Logger* logger) const {
            if (logger->is_terminal_output()) {
                logger->log(enchanced.message.level, "{}", styled(enchanced.message.content, emphasis::bold));
            } else {
                logger->log(enchanced.message.level, enchanced.message.content);
            }
            if (!enchanced.info)
                return;
            std::size_t padding = std::to_string(enchanced.info->line_number).size() + 1;
            // location line
            bite::print(logger->raw_ostream(), "{}", repeated(" ", padding));
            if (logger->is_terminal_output()) {
                bite::print(foreground(terminal_color::bright_black), logger->raw_ostream(), "┌─> ");
            } else {
                bite::print(logger->raw_ostream(), "--> ");
            }
            bite::println(logger->raw_ostream(), "{}", enchanced.message.inline_msg->location.file_path);
            // spacer
            bite::print(logger->raw_ostream(), "{}", repeated(" ", padding));
            if (logger->is_terminal_output()) {
                bite::println(foreground(terminal_color::bright_black), logger->raw_ostream(), "│");
            } else {
                bite::println(logger->raw_ostream(), "|");
            }
            // code line
            if (logger->is_terminal_output()) {
                bite::println(
                    logger->raw_ostream(),
                    "{} {} {}",
                    styled(enchanced.info->line_number, emphasis::bold),
                    styled("│", foreground(terminal_color::bright_black)),
                    enchanced.info->line
                );
            } else {
                bite::println(logger->raw_ostream(), "{} | {}", enchanced.info->line_number, enchanced.info->line);
            }
            // inline hint spacer
            bite::print(logger->raw_ostream(), "{}", repeated(" ", padding));
            if (logger->is_terminal_output()) {
                bite::print(foreground(terminal_color::bright_black), logger->raw_ostream(), "│");
            } else {
                bite::print(logger->raw_ostream(), "|");
            }
            // inline hint message
            bite::print(logger->raw_ostream(), "{}", repeated(" ", enchanced.info->in_line_start + 1));
            if (logger->is_terminal_output()) {
                bite::println(
                    foreground(Logger::level_color(enchanced.message.level)),
                    logger->raw_ostream(),
                    "{} {}",
                    repeated(
                        "^",
                        enchanced.message.inline_msg->location.end_offset - enchanced.message.inline_msg->location.
                        start_offset
                    ),
                    enchanced.message.inline_msg->content
                );
            } else {
                bite::println(
                    logger->raw_ostream(),
                    "{} {}",
                    repeated(
                        "^",
                        enchanced.message.inline_msg->location.end_offset - enchanced.message.inline_msg->location.
                        start_offset
                    ),
                    enchanced.message.inline_msg->content
                );
            }
        }
    };
}

#endif //ENHANCE_MESSAGES_H
