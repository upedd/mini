//
// Created by MiłoszK on 22.06.2024.
//

#include "TimeCall.h"

int TimeCall::arity() {
    return 0;
}

std::any TimeCall::call(Interpreter *interpreter, std::vector<std::any> arguments) {
    return static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}