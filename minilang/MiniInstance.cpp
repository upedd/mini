//
// Created by MiłoszK on 23.06.2024.
//

#include "MiniInstance.h"

std::any MiniInstance::get(const Token &token) {
    if (fields.contains(token.lexeme)) {
        return fields[token.lexeme];
    }

    throw RuntimeError(token, "Undefined property '" + token.lexeme + "'.");
}

void MiniInstance::set(const Token &name, const std::any &value) {
    fields[name.lexeme] = value;
}
