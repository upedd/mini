#include <chrono>
#include <fstream>
#include <iostream>
#include "Compiler.h"
#include "debug.h"
#include "VM.h"
#include "shared/SharedContext.h"
// TODO: attributes
// TODO: duplicates by traits
// TODO: this closures
// TODO: proper globals
// TODO: imports
// TODO: refactor

// TODO: investigate fun in class infinite loop?
int main(int argc, char** argv) {
    // TODO error handling
    if (argc != 2) {
        std::cerr << "Usage: ./bite [path to bite file]\n";
        return -1;
    }
    SharedContext context { bite::Logger(std::cout, true) };
    Module* main_module = context.compile(argv[1]);
    context.execute(*main_module);

    // auto result = vm.run();
    // if (!result) {
    //     std::cerr << result.error().what() << '\n';
    // }
}
