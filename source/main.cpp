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
    std::string source = ss.str();
    Compiler compiler(source);
    compiler.compile();
    auto& func = compiler.get_main();
    VM vm(&func);
    vm.adopt_objects(compiler.allocated_objects);
    vm.gc_ready = true;
    if (auto result = vm.run()) {
        std::cout << "Your bite program output: " << result->to_string() << '\n';
    } else {
        std::cout << "Exectuion error!\n";
    }
}
