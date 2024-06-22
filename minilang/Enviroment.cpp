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

std::shared_ptr<Enviroment> Enviroment::ancestor(int distance) {
    auto env = std::make_shared<Enviroment>(*this);
    for (int i = 0; i < distance; ++i) {
        env = env->enclosing;
    }
    return env;
}

std::any Enviroment::get_at(int distance, const std::string &name) {
    return ancestor(distance)->values[name];
}

void Enviroment::assign_at(int distance, const Token &name, const std::any &value) {
    ancestor(distance)->values[name.lexeme] = value;
}
