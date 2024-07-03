#include <fstream>
#include <iostream>
#include <sstream>

#include "Compiler.h"
#include "Lexer.h"
#include "Parser.h"
#include "VM.h"

int main() {
    // TODO better strings handling!

    std::ifstream in("test.bite");
    std::stringstream ss;
    ss << in.rdbuf();
    std::string source = ss.str();

    // Lexer lexer(source);
    // Parser parser(source);
    // //
    // std::vector<Stmt> stmts = parser.parse();
    // for (auto& error : parser.get_errors()) {
    //     std::cerr << "Error at: " << error.token.to_string(source) << " Message: " << error.message << '\n';
    // }
    // for (auto& stmt : stmts) {
    //     std::cout << stmt_to_string(stmt, source) << '\n';
    // }
    //if (parser.get_errors().empty()) {
        Compiler compiler(source);
        compiler.compile();
        //code_gen.generate(stmts, source);
        auto func = compiler.get_main();
        VM vm(&func);
        vm.run();
    //}
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
