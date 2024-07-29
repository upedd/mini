#ifndef ERRORPRETTYPRINTER_H
#define ERRORPRETTYPRINTER_H
#include <fstream>
#include <iostream>
#include <list>

#include "Lexer.h"


class ErrorPrettyPrinter {
public:
    ErrorPrettyPrinter(const std::string& path, std::list<Lexer::Error> errors) : errors(std::move(errors)),
        source_file(path),
        file_path(path) {}

    struct Message {
        std::string line;
        int line_offset;
        int line_number;
        std::string error_message;
        std::string inline_message;
    };

    void prepare_messages() {
        int line_counter = 0;
        std::string current_line;
        std::size_t previous_line_offset = 0;
        while (std::getline(source_file, current_line)) {
            ++line_counter;
            std::size_t current_line_offset = source_file.tellg();
            std::erase_if(
                errors,
                [&](const Lexer::Error& error) {
                    if (error.source_offset < current_line_offset) {
                        messages.emplace_back(
                            current_line,
                            error.source_offset - previous_line_offset,
                            line_counter,
                            error.message,
                            error.message
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

    template<typename... Args>
    void print(std::format_string<Args...> string, Args&&... args) {
        std::print(std::cout, string, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void println(std::format_string<Args...> string, Args&&... args) {
        std::println(std::cout, string, std::forward<Args>(args)...);
    }
    template<typename... Args>
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
            print("-> ");
        } else {
            print("┌─ ");
        }
        println("{}:{}:{}", path, line_number, line_offset + 1);
    }

    void print_messages() {
        for (auto& msg : messages) {
            std::size_t padding = std::to_string(msg.line_number).size() + 1;
            print("\033[1;31mbold red text\033[0m");
            println( "error: {}", msg.error_message);
            print_padding(padding);
            print_location(file_path, msg.line_number, msg.line_offset);
            print_padding(padding);
            println( "│");
            println( "{} │ {}", msg.line_number, msg.line);
            print_padding(padding);
            print( "│ ");
            print_padding(msg.line_offset);
            print_repeat(4, "^");
            println(" {}", msg.inline_message);
            // std::cout << "Error: " << msg.error_message << '\n';
            // std::cout << "  " << corner_top_left << hbar << ' ' << file_path << ':' << msg.line_number << ':' << msg.line_offset << '\n';
            // std::cout << "  |\n";
            // // TODO: dynamic spacing for line number
            // // TODO: truncate source code if nedded
            // std::cout << msg.line_number << " |" << ' ' << msg.line << '\n';
            // std::cout << "  |\n";
            // std::cout << '\n';
        }
    }


    // should be sorted by deafult
    std::vector<Message> messages;
    std::list<Lexer::Error> errors;
    std::ifstream source_file;
    std::string file_path;
    bool terminal_compatibility_mode = false;
};

#endif //ERRORPRETTYPRINTER_H
