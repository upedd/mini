#include <fstream>
#include <iostream>
#include <sstream>

#include "Compiler.h"
#include "VM.h"

vm::VM::InterpretResult interpret(std::string_view source) {
    vm::VM machine;
    vm::Compiler compiler(machine, source);

    if (!compiler.compile()) {
        return vm::VM::InterpretResult::COMPILE_ERROR;
    }

    vm::Chunk& chunk = compiler.get_chunk();



    vm::VM::InterpretResult result = machine.interpret(&chunk);

    return result;
}

void repl() {
    std::string line;
    while (true) {
        std::cout << '>';
        if (std::cin >> line) {
            interpret(line);
        } else break;
    }
}

void run_file(std::string_view path) {
    std::ifstream fs(path.data());
    // TODO: check errors
    std::stringstream ss;
    ss << fs.rdbuf();
    std::string source = ss.str();
    auto result = interpret(source);
    if (result == vm::VM::InterpretResult::COMPILE_ERROR) exit(65);
    if (result == vm::VM::InterpretResult::RUNTIME_ERROR) exit(70);
}

int main(int argc, const char* argv[]) {
    vm::VM virutal_machine;
    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        run_file(argv[1]);
    } else {
        std::cerr << "Usage: mini [path]\n";
        return 64;
    }
}
