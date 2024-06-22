#include "Enviroment.h"

void Enviroment::define(const std::string &name, const std::any &value) {
    values[name] = value;
}

std::any Enviroment::get(const Token &name) {
    if (values.contains(name.lexeme)) {
        return values[name.lexeme];
    }

    if (enclosing != nullptr) return enclosing->get(name);

    throw RuntimeError(name, "Undefined variable '" + name.lexeme + "'.");
}

void Enviroment::assign(const Token &name, const std::any &value) {
    if (values.contains(name.lexeme)) {
        values[name.lexeme] = value;
        return;
    }

    if (enclosing != nullptr) {
        enclosing->assign(name, value);
        return;
    }


    throw RuntimeError(name, "Undefined variable '" + name.lexeme + "'.");
}
