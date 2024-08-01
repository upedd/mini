#include <chrono>
#include <fstream>
#include <iostream>
#include "Compiler.h"
#include "VM.h"
#include "shared/SharedContext.h"

int main(int argc, char** argv) {
    // TODO error handling
    if (argc != 2) {
        std::cerr << "Usage: ./bite [path to bite file]\n";
        return -1;
    }
    SharedContext context { bite::Logger(std::cout, true) };
    Compiler compiler(bite::file_input_stream(argv[1]), &context);
    if (!compiler.compile()) {
        return 1;
    }
    auto& func = compiler.get_main();
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
