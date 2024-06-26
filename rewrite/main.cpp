#include <fstream>
#include <iostream>
#include <sstream>

#include "Lexer.h"

int main() {
    std::ifstream in("test.bite");
    std::stringstream ss;
    ss << in.rdbuf();
    std::string source = ss.str();

    Lexer lexer(source);

    while (true) {
        auto token = lexer.next_token();
        if (!token) {
            std::cerr << token.error().message;
            break;
        } else {
            if (token->type == Token::Type::END) break;
            std::cout << token->to_string(source) << '\n';
        }
    }
}
