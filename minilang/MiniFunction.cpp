//
// Created by MiłoszK on 22.06.2024.
//

#include "MiniFunction.h"
#include "MiniCallable.h"
#include "Return.h"

std::any MiniFunction::call(Interpreter *interpreter, std::vector<std::any> arguments) {
    Enviroment env(closure);

    for (int i = 0; i < declaration->params.size(); ++i) {
        env.define(declaration->params[i].lexeme, arguments[i]);
    }

    try {
        interpreter->execute_block(declaration->body, env);
    } catch (Return& return_value) {
        if (is_initializer) return closure->get_at(0, "this");
        return return_value.value;
    }

    if (is_initializer) return closure->get_at(0, "this");
    return nullptr;
}

int MiniFunction::arity() {
    return declaration->params.size();
}
// TODO memory leak
MiniFunction* MiniFunction::bind(MiniInstance *instance) {
    auto env = make_shared<Enviroment>(closure);
    env->define("this", instance);
    return new MiniFunction(declaration, env, is_initializer);
}
