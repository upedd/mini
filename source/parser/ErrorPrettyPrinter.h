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


    void print_messages() {
        std::wcout << L"┌────";
        std::cout << "abc";
        std::print(std::cout, "abc");

        std::string vbar = terminal_compatibility_mode ? "|" : "│";
        std::string hbar = terminal_compatibility_mode ? "-" : "─";
        std::string corner_top_left = terminal_compatibility_mode ? "-" : "┌";

        for (auto& msg : messages) {
            std::cout << "Error: " << msg.error_message << '\n';
            std::cout << "  " << corner_top_left << hbar << ' ' << file_path << ':' << msg.line_number << ':' << msg.line_offset << '\n';
            std::cout << "  |\n";
            // TODO: dynamic spacing for line number
            // TODO: truncate source code if nedded
            std::cout << msg.line_number << " |" << ' ' << msg.line << '\n';
            std::cout << "  |\n";
            std::cout << '\n';
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
