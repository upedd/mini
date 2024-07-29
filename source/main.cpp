#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Compiler.h"
#include "VM.h"

int main(int argc, char** argv) {
    // TODO error handling
    if (argc != 2) {
        std::cerr << "Usage: ./bite [path to bite file]\n";
        return -1;
    }
    std::ifstream in(argv[1]);
    std::stringstream ss;
    ss << in.rdbuf();
    SharedContext context;
    std::string source = ss.str();
    Compiler compiler(source, &context);
    compiler.compile();
    auto& func = compiler.get_main();
    // Disassembler disassembler(func);
    // disassembler.disassemble("main");
    auto& functions = compiler.get_functions();
    GarbageCollector gc;
    for (auto* function : functions) {
        gc.add_object(function);
    }
    VM vm(std::move(gc), &func);
    vm.add_native_function("print", [](const std::vector<Value>& args) {
        std::cout << args[1].to_string() << '\n';
        return nil_t;
    });
    auto result = vm.run();
    if (!result) {
        std::cerr << result.error().what() << '\n';
    }
}
