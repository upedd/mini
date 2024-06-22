//
// Created by MiłoszK on 22.06.2024.
//

#include "MiniFunction.h"

std::any MiniFunction::call(Interpreter *interpreter, std::vector<std::any> arguments) {
    Enviroment env(&interpreter->globals);

    for (int i = 0; i < declaration->params.size(); ++i) {
        env.define(declaration->params[i].lexeme, arguments[i]);
    }

    interpreter->execute_block(declaration->body, env);
    return nullptr;
}

int MiniFunction::arity() {
    return declaration->params.size();
}
