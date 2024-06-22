//
// Created by MiłoszK on 19.06.2024.
//

#include "Mini.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>

#include "AstPrinter.h"
#include "Parser.h"
#include "Resolver.h"
#include "Scanner.h"

void Mini::run(const std::string_view input) {
    Scanner scanner(input);
    std::vector<Token> tokens = scanner.scan_tokens();
    Parser parser(tokens);
    auto expr = parser.parse();
    if (had_error) return;

    Resolver resolver(&interpreter);
    resolver.resolve(expr);
    if (had_error) return;

    interpreter.interpret(expr);
}

void Mini::run_prompt() {
    std::string input;
    while (true) {
        std::cout << "> ";
        std::cin >> input;
        if (input.empty()) break;
        run(input);
        had_error = false;
    }
}

void Mini::run_file(const std::string_view filename) {
    std::ifstream fs(filename.data());
    // TODO: check errors
    std::stringstream ss;
    ss << fs.rdbuf();
    if (had_error) exit(65);
    if (had_runtime_error) exit(70);
    run(ss.str());
}

void Mini::runtime_error(const RuntimeError &error) {
    std::cout << error.what() << "\n[line " << error.token.line << "]\n";
}

void Mini::error(const int line, const std::string_view message) {
    report(line, "", message);
}

void Mini::error(const Token &token, std::string_view message) {
    if (token.type == TokenType::END) {
        report(token.line, " at end", message);
    } else {
        report(token.line, std::string(" at '") + token.lexeme + "'", message);
    }
}

void Mini::report(const int line, const std::string_view where, const std::string_view message) {
    std::cout << "[line " << line << "] Error" << where << ": " << message << '\n';
    had_error = true;
}
