#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Compiler.h"
#include "VM.h"
#include "parser/ErrorPrettyPrinter.h"
#include "shared/SharedContext.h"

int main(int argc, char** argv) {
    // TODO error handling
    if (argc != 2) {
        std::cerr << "Usage: ./bite [path to bite file]\n";
        return -1;
    }
    SharedContext context;
    Compiler compiler(FileInputStream(argv[1]), &context);
    compiler.compile();
    auto& func = compiler.get_main();
    std::vector<Lexer::Error> errors = context.get_errors();
    if (!errors.empty()) {
        ErrorPrettyPrinter printer(argv[1], std::list(errors.begin(), errors.end()));
        printer.prepare_messages();
        printer.print_messages();
        int a;
        std::cin >> a;
    }
    // Disassembler disassembler(func);
    // disassembler.disassemble("main");
    auto& functions = compiler.get_functions();
    GarbageCollector gc;
    for (auto* function : functions) {
        gc.add_object(function);
    }
    VM vm(std::move(gc), &func);
    vm.add_native_function(
        "print",
        [](const std::vector<Value>& args) {
            std::cout << args[1].to_string() << '\n';
            return nil_t;
        }
    );
    auto result = vm.run();
    if (!result) {
        std::cerr << result.error().what() << '\n';
    }
}
