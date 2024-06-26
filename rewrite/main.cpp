#include <fstream>
#include <iostream>
#include <sstream>

#include "Lexer.h"
#include "Parser.h"

int main() {
    std::ifstream in("test.bite");
    std::stringstream ss;
    ss << in.rdbuf();
    std::string source = ss.str();

    Lexer lexer(source);
    Parser parser(lexer);
    //
    Expr expr = parser.expression();
    for (auto& error : parser.get_errors()) {
        std::cerr << "Error at: " << error.token.to_string(source) << " Message: " << error.message << '\n';
    }
    if (parser.get_errors().empty()) {
        std::cout << to_string(expr) << '\n';
    }
    // while (true) {
    //     auto token = lexer.next_token();
    //     if (!token) {
    //         std::cerr << token.error().message;
    //         break;
    //     } else {
    //         if (token->type == Token::Type::END) break;
    //         std::cout << token->to_string(source) << '\n';
    //     }
    // }
}
