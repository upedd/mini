//
// Created by MiłoszK on 22.06.2024.
//

#include "MiniFunction.h"

#include "Return.h"

std::any MiniFunction::call(Interpreter *interpreter, std::vector<std::any> arguments) {
    Enviroment env(interpreter->globals.get());

    for (int i = 0; i < declaration->params.size(); ++i) {
        env.define(declaration->params[i].lexeme, arguments[i]);
    }

    try {
        interpreter->execute_block(declaration->body, env);
    } catch (Return& return_value) {
        return return_value.value;
    }

    return nullptr;
}

int MiniFunction::arity() {
    return declaration->params.size();
}
