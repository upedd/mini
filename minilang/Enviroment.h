#ifndef ENVIROMENT_H
#define ENVIROMENT_H
#include <any>
#include <string>
#include <unordered_map>

#include "RuntimeError.h"
#include "Token.h"
#include "generated/Expr.h"


class Enviroment {
public:
    Enviroment() = default;
    explicit Enviroment(Enviroment* enclosing) : enclosing(enclosing) {}

    void define(const std::string& name, const std::any& value);

    std::any get(const Token& name);

    void assign(const Token & name, const std::any & value);

private:
    std::unordered_map<std::string, std::any> values;
    Enviroment* enclosing = nullptr;
};



#endif //ENVIROMENT_H
