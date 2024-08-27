#include "SharedContext.h"

#include "../Analyzer.h"
#include "../parser/Parser.h"

Module* SharedContext::get_module(StringTable::Handle name) {
    if (modules.contains(name)) {
        return &modules[name];
    }
    if (std::filesystem::exists(*name)) {
        Parser parser {bite::file_input_stream(*name), this};
        ast_storage[name] = parser.parse();
        if (parser.has_errors()) {
            diagnostics.print(std::cout);
            return nullptr;
        }
        bite::Analyzer analyzer {this};
        analyzer.analyze(ast_storage[name]);
        if (analyzer.has_errors()) {
            diagnostics.print(std::cout);
            return nullptr;
        }


    }
    return nullptr;
}
