#ifndef ERRORPRETTYPRINTER_H
#define ERRORPRETTYPRINTER_H
#include <filesystem>

#include "CompilationMessage.h"
#include "../shared/Message.h"

namespace bite {
    struct EnchancedInfo {
        std::string line;
        std::size_t in_line_start;
    };

    struct EnchancedMessage {
        Message message;
        std::optional<EnchancedInfo> info;
    };

    namespace detail {
        inline void enchance_messages_from_source(
            std::vector<EnchancedMessage>& output,
            const std::string& path,
            const std::vector<Message const*>& messages
        ) {
            std::ifstream file(path);
            int line_counter = 0;
            std::string current_line;
            std::size_t previous_line_offset = 0;
            while (std::getline(file, current_line)) {
                ++line_counter;
                std::size_t current_line_offset = previous_line_offset + current_line.size() + 1; // why plus one here?
                for (auto* message : messages) {
                    std::size_t start_offset = message->inline_msg->location.start_offset;
                    if (start_offset < current_line_offset) {
                        output.emplace_back(
                            *message,
                            EnchancedInfo { .line = current_line, .in_line_start = start_offset - previous_line_offset }
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
            if (message.inline_msg) {
                // Cannot enchance messages without inline messages
                output[i] = { message };
            } else {
                // Proccess file
                processed[i] = true;
                std::vector<Message const*> same_source_messages;
                same_source_messages.push_back(&message);
                auto& message_path = message.inline_msg->location.file_path;
                for (std::size_t j = i + 1; j < messages.size(); ++j) {
                    if (messages[j].inline_msg && messages[j].inline_msg->location.file_path == message_path) {
                        processed[j] = true;
                        same_source_messages.push_back(&messages[j]);
                    }
                }
                detail::enchance_messages_from_source(output, message_path, same_source_messages);
            }
        }
        return output;
    }
}

// class ErrorPrettyPrinter {
// public:
//     ErrorPrettyPrinter(const std::string& path, std::list<CompilationMessage> errors) : errors(std::move(errors)),
//         source_file(path),
//         file_path(path) {
//         prepare_messages();
//     }
//
//     struct Message {
//         std::string line;
//         int line_offset_start;
//         int line_offset_end;
//         int line_number;
//         std::string reason;
//         std::string inline_message;
//     };
//
//     void prepare_messages() {
//         int line_counter = 0;
//         std::string current_line;
//         std::size_t previous_line_offset = 0;
//         while (std::getline(source_file, current_line)) {
//             ++line_counter;
//             std::size_t current_line_offset = previous_line_offset + current_line.size() + 1; // why plus one here?
//             std::erase_if(
//                 errors,
//                 [&](const CompilationMessage& error) {
//                     if (error.source_offset_start < current_line_offset) {
//                         messages.emplace_back(
//                             current_line,
//                             error.source_offset_start - previous_line_offset,
//                             error.source_offset_end - previous_line_offset,
//                             line_counter,
//                             error.reason,
//                             error.inline_message
//                         );
//                         return true;
//                     }
//                     return false;
//                 }
//             );
//             previous_line_offset = current_line_offset;
//         }
//     }
//
//     void print_padding(std::size_t padding) {
//         bite::print("{}", bite::repeated(" ", padding));
//     }
//
//     void print_location(std::string_view path, std::size_t line_number, std::size_t line_offset) {
//         if (terminal_compatibility_mode) {
//             bite::print("--> ");
//         } else {
//             bite::print("\033[90m┌─>\033[0m ");
//         }
//         bite::println("{}:{}:{}", path, line_number, line_offset + 1);
//     }
//
//     void print_main_message(std::string_view message) {
//         if (terminal_compatibility_mode) {
//             bite::println("error: {}", message);
//         } else {
//             bite::println("\033[1;31merror\033[0m\033[1m: {}\033[0m", message);
//         }
//     }
//
//     void print_vbar() {
//         if (terminal_compatibility_mode) {
//             bite::print("|");
//         } else {
//             bite::print("\033[90m│\033[0m");
//         }
//     }
//
//     // truncating main line
//     void print_main_line(std::size_t line_number, std::string_view line) {
//         if (terminal_compatibility_mode) {
//             bite::println("{} | {}", line_number, line);
//         } else {
//             bite::println("\033[1m{}\033[0m \033[90m│\033[0m {}", line_number, line);
//         }
//     }
//
//     void print_inline_message(std::size_t token_length, std::string_view message) {
//         if (!terminal_compatibility_mode) {
//             bite::print("\033[31m");
//         }
//         bite::print("{} {}", bite::repeated("^", token_length), message);
//         if (!terminal_compatibility_mode) {
//             bite::println("\033[0m");
//         } else {
//             bite::println("");
//         }
//     };
//
//     void print_messages() {
//         for (auto& msg : messages) {
//             std::size_t padding = std::to_string(msg.line_number).size() + 1;
//             print_main_message(msg.reason);
//
//             print_padding(padding);
//             print_location(file_path, msg.line_number, msg.line_offset_start);
//
//             print_padding(padding);
//             print_vbar();
//             bite::println(" ");
//
//             print_main_line(msg.line_number, msg.line);
//
//             print_padding(padding);
//             print_vbar();
//             print_padding(msg.line_offset_start + 1);
//             print_inline_message(msg.line_offset_end - msg.line_offset_start, msg.inline_message);
//         }
//     }
//
//     // should be sorted by deafult
//     SharedContext* context;
//     std::vector<Message> messages;
//     std::list<CompilationMessage> errors;
//     std::ifstream source_file;
//     std::string file_path;
//     bool terminal_compatibility_mode = false;
// };

#endif //ERRORPRETTYPRINTER_H
