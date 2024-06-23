//
// Created by MiłoszK on 23.06.2024.
//

#include "MiniInstance.h"

#include "MiniFunction.h"

std::any MiniInstance::get(const Token &token) {
    if (fields.contains(token.lexeme)) {
        return fields[token.lexeme];
    }

    MiniFunction* method = klass.find_method(token.lexeme);
    if (method != nullptr) return method->bind(this);

    throw RuntimeError(token, "Undefined property '" + token.lexeme + "'.");
}

void MiniInstance::set(const Token &name, const std::any &value) {
    fields[name.lexeme] = value;
}
