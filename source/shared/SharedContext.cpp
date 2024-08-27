#include "SharedContext.h"

#include "../Analyzer.h"
#include "../Compiler.h"
#include "../parser/Parser.h"
#include "../VM.h"

Value FunctionContext::get_arg(int64_t pos) {
    return vm->stack[frame_pointer + pos + 1];
}

// TODO: circular?
Module* SharedContext::get_module(StringTable::Handle name) {
    if (modules.contains(name)) {
        return &modules[name];
    }
    if (std::filesystem::exists(*name)) {
        return compile(*name);
    }
    return nullptr;
}

Module* SharedContext::compile(const std::string& name) {
    Parser parser { bite::file_input_stream(name), this };
    ast_storage.push_back(parser.parse());
    auto& ast = ast_storage.back();
    if (parser.has_errors()) {
        diagnostics.print(std::cout, true);
        return nullptr;
    }
    bite::Analyzer analyzer { this };
    analyzer.analyze(ast);
    if (analyzer.has_errors()) {
        diagnostics.print(std::cout, true);
        return nullptr;
    }
    Compiler compiler { this };
    compiler.compile(&ast);
    for (auto* function : compiler.get_functions()) {
        gc.add_object(function);
    }
    // Bite automatically exports all globals declarations, this can change in future.
    modules[intern(name)] = Module {
            .m_was_executed = false,
            .function = compiler.get_main(),
            .declarations = std::move(ast.enviroment.globals),
            .values = {}
        };
    return &modules[intern(name)];
}

void SharedContext::add_module(const StringTable::Handle& name) {
    modules[name] = Module();
}

void SharedContext::execute(Module& module) {
    module.m_was_executed = true;
    running_vms.emplace_back(&gc, module.function, this);
    // TODO: handle errors
    running_vms.back().add_native_function(
        "print",
        [](const std::vector<Value>& args) {
            std::cout << args[1].to_string() << '\n';
            return nil_t;
        }
    );

    auto result = running_vms.back().run();
    if (!result) {
        logger.log(bite::Logger::Level::error, "uncaught error: {}", result.error().what());
    }

    // TODO: temp
    for (auto& [name, value] : running_vms.back().globals) {
        module.values[intern(name)] = value;
    }
    running_vms.pop_back();
}

Value SharedContext::get_value_from_module(const StringTable::Handle& module, const StringTable::Handle& name) {
    BITE_ASSERT(modules.contains(module));
    if (!modules[module].m_was_executed) {
        execute(modules[module]);
    }
    BITE_ASSERT(modules[module].values.contains(name));
    return modules[module].values[name];
}

void SharedContext::run_gc() {
    for (auto& vm : running_vms) {
        vm.mark_roots_for_gc();
    }
    for (auto& [_, module] : modules) {
        if (module.m_was_executed) {
            for (auto& [_, value] : module.values) {
                gc.mark(value);
            }
        } else {
            gc.mark(module.function);
        }
    }
    gc.collect();
}
