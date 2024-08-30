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
        return modules[name].get();
    }
    if (std::filesystem::exists(*name)) {
        return compile(*name);
    }
    return nullptr;
}

FileModule* SharedContext::compile(const std::string& name) {
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
    bite::unordered_dense::map<StringTable::Handle, Declaration*> declarations;
    for (auto& [name, global] : ast.enviroment.globals) {
        declarations[name] = global.declaration;
    }
    modules[intern(name)] = std::make_unique<FileModule>(compiler.get_main(), std::move(declarations));
    return static_cast<FileModule*>(modules[intern(name)].get());
}

void SharedContext::add_module(const StringTable::Handle name, std::unique_ptr<ForeignModule> module) {
    modules[name] = std::move(module);
}

void SharedContext::execute(FileModule& module) {
    module.m_was_executed = true;
    running_vms.emplace_back(&gc, module.function, this);
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

// TODO: temp
std::vector<std::pair<StringTable::Handle, Value>> SharedContext::get_value_from_module(
    const StringTable::Handle& module,
    const StringTable::Handle& name
) {
    BITE_ASSERT(modules.contains(module));
    if (auto* file_module = dynamic_cast<FileModule*>(modules[module].get())) {
        if (!file_module->m_was_executed) {
            execute(*file_module);
        }
        std::vector<std::pair<StringTable::Handle, Value>> values;
        for (auto& [value_name, value] : file_module->values) {
            // TODO: fixme!
            if (value_name->starts_with(*name + "::")) {
                values.emplace_back(value_name, value);
            } else if (value_name == name) {
                values.emplace_back(value_name, value);
            }
        }
        return values;
    }
    // if (auto* foreign_module = dynamic_cast<ForeignModule*>(modules[module].get())) {
    //     BITE_ASSERT(foreign_module->functions.contains(name));
    //     return &foreign_module->functions[name];
    // }
    BITE_PANIC("unknown module type");
}

void SharedContext::run_gc() {
    for (auto& vm : running_vms) {
        vm.mark_roots_for_gc();
    }
    for (auto& [_, module] : modules) {
        if (auto* file_module = dynamic_cast<FileModule*>(module.get())) {
            if (file_module->m_was_executed) {
                for (auto& [_, value] : file_module->values) {
                    gc.mark(value);
                }
            } else {
                gc.mark(file_module->function);
            }
        }
    }
    gc.collect();
}
