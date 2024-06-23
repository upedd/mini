//
// Created by MiłoszK on 23.06.2024.
//

#include "Compiler.h"

#include <iostream>

#include "Scanner.h"

void compile(std::string_view source) {
    Scanner scanner(source);
    while (true) {
        Token token = scanner.scan_token();
        std::cout << static_cast<int>(token.type) << ' ' <<  token.lexeme << '\n';
        if (token.type == Token::Type::END) break;
    }
}
