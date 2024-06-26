#include <iostream>

#include "Lexer.h"

int main() {
    Lexer lexer("a := 4 + 3 * (12 + 3);\n for (i in 3..4) {\n    print(a, i);\n}");

    while (true) {
        auto token = lexer.next_token();
        if (token.type == Token::Type::END) {
            break;
        }
        std::cout << static_cast<int>(token.type) << '\n';
    }
}
