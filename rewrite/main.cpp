#include <iostream>

#include "Lexer.h"

int main() {
    Lexer lexer("a := 4 + 3 * (12.5 + 3);\n loop (i in 3..4) {\n    print(a, i, \"abcd\");\n}");

    while (true) {
        auto token = lexer.next_token();
        if (token->type == Token::Type::END) {
            break;
        }
        std::cout << Token::type_to_string(token->type) << '\n';
    }
}
