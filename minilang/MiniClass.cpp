//
// Created by MiłoszK on 23.06.2024.
//

#include "MiniClass.h"

#include "MiniInstance.h"

std::any MiniClass::call(Interpreter *interpreter, std::vector<std::any> arguments) {
    // TODO memory leak
    auto* instance = new MiniInstance(*this);
    return instance;
}

int MiniClass::arity() {
    return 0;
}
