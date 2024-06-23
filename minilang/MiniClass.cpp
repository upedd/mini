//
// Created by MiłoszK on 23.06.2024.
//

#include "MiniClass.h"

#include "MiniInstance.h"

std::any MiniClass::call(Interpreter *interpreter, std::vector<std::any> arguments) {
    // TODO memory leak
    auto* instance = new MiniInstance(*this);
    auto* initializer = find_method("init");
    if (initializer != nullptr) {
        initializer->bind(instance)->call(interpreter, arguments);
    }
    return instance;
}

int MiniClass::arity() {
    auto* initializer = find_method("init");
    if (initializer != nullptr) {
        return initializer->arity();
    }
    return 0;
}

MiniFunction* MiniClass::find_method(const std::string &name) {
    if (methods.contains(name)) {
        return methods[name];
    }
    return nullptr;
}
