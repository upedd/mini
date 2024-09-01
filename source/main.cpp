#include <chrono>
#include <fstream>
#include <iostream>
#include "Compiler.h"
#include "shared/SharedContext.h"
// TODO: cyclic imports
// TODO: refactor

// TODO: investigate fun in class infinite loop?
int main(int argc, char** argv) {
    // TODO error handling
    if (argc != 2) {
        std::cerr << "Usage: ./bite [path to bite file]\n";
        return -1;
    }
    SharedContext context { bite::Logger(std::cout, true) };

    auto os_module = std::make_unique<ForeignModule>();
    auto print_symbol = context.intern("print");
    auto& cout = std::cout;
    os_module->functions[print_symbol] = {
            .arity = 1,
            .name = print_symbol,
            .function = [&cout](FunctionContext ctx) {
                // TODO: temp
                std::vprint_unicode(cout, ctx.get_arg(0).to_string(), {});
                std::cout << '\n';
                return nil_t;
            }
        };

    context.add_module(context.intern("os"), std::move(os_module));
    FileModule* main_module = context.compile(argv[1]);
    if (!main_module) {
        return -1;
    }
    context.execute(*main_module);
}
