#ifndef ERRORPRETTYPRINTER_H
#define ERRORPRETTYPRINTER_H
#include <fstream>
#include <iostream>
#include <list>

#include "CompilationMessage.h"

class ErrorPrettyPrinter {
public:
    ErrorPrettyPrinter(const std::string& path, std::list<CompilationMessage> errors) : errors(std::move(errors)),
        source_file(path),
        file_path(path) {
        prepare_messages();
    }

    struct Message {
        std::string line;
        int line_offset_start;
        int line_offset_end;
        int line_number;
        std::string reason;
        std::string inline_message;
    };

    void prepare_messages() {
        int line_counter = 0;
        std::string current_line;
        std::size_t previous_line_offset = 0;
        while (std::getline(source_file, current_line)) {
            ++line_counter;
            std::size_t current_line_offset = previous_line_offset + current_line.size() + 1; // why plus one here?
            std::erase_if(
                errors,
                [&](const CompilationMessage& error) {
                    if (error.source_offset_start < current_line_offset) {
                        messages.emplace_back(
                            current_line,
                            error.source_offset_start - previous_line_offset,
                            error.source_offset_end - previous_line_offset,
                            line_counter,
                            error.reason,
                            error.inline_message
                        );
                        return true;
                    }
                    return false;
                }
            );
            previous_line_offset = current_line_offset;
        }
    }

    // These should be in std but are missing? https://en.cppreference.com/w/cpp/io/print

    template <typename... Args>
    void print(std::format_string<Args...> string, Args&&... args) {
        std::print(std::cout, string, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void println(std::format_string<Args...> string, Args&&... args) {
        std::println(std::cout, string, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void print_repeat(std::size_t times, std::format_string<Args...> string, Args&&... args) {
        for (std::size_t i = 0; i < times; ++i) {
            std::print(std::cout, string, std::forward<Args>(args)...);
        }
    }

    void print_padding(std::size_t padding) {
        print_repeat(padding, " ");
    }

    void print_location(std::string_view path, std::size_t line_number, std::size_t line_offset) {
        if (terminal_compatibility_mode) {
            print("--> ");
        } else {
            print("\033[90m┌─>\033[0m ");
        }
        println("{}:{}:{}", path, line_number, line_offset + 1);
    }

    void print_main_message(std::string_view message) {
        if (terminal_compatibility_mode) {
            println("error: {}", message);
        } else {
            println("\033[1;31merror\033[0m\033[1m: {}\033[0m", message);
        }
    }

    void print_vbar() {
        if (terminal_compatibility_mode) {
            print("|");
        } else {
            print("\033[90m│\033[0m");
        }
    }

    // truncating main line
    void print_main_line(std::size_t line_number, std::string_view line) {
        if (terminal_compatibility_mode) {
            println("{} | {}", line_number, line);
        } else {
            println("\033[1m{}\033[0m \033[90m│\033[0m {}", line_number, line);
        }
    }

    void print_inline_message(std::size_t token_length, std::string_view message) {
        if (!terminal_compatibility_mode) {
            print("\033[31m");
        }
        print_repeat(token_length, "^");
        print(" {}", message);
        if (!terminal_compatibility_mode) {
            println("\033[0m");
        } else {
            println("");
        }
    };

    void print_messages() {
        for (auto& msg : messages) {
            std::size_t padding = std::to_string(msg.line_number).size() + 1;
            print_main_message(msg.reason);

            print_padding(padding);
            print_location(file_path, msg.line_number, msg.line_offset_start);

            print_padding(padding);
            print_vbar();
            println("");

            print_main_line(msg.line_number, msg.line);

            print_padding(padding);
            print_vbar();
            print_padding(msg.line_offset_start + 1);
            print_inline_message(msg.line_offset_end - msg.line_offset_start, msg.inline_message);
        }
    }

    // should be sorted by deafult
    std::vector<Message> messages;
    std::list<CompilationMessage> errors;
    std::ifstream source_file;
    std::string file_path;
    bool terminal_compatibility_mode = false;
};

#endif //ERRORPRETTYPRINTER_H
