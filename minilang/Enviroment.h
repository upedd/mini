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
    explicit Enviroment(std::shared_ptr<Enviroment> enclosing) : enclosing(std::move(enclosing)) {}

    void define(const std::string& name, const std::any& value);

    std::any get(const Token& name);

    void assign(const Token & name, const std::any & value);

    std::shared_ptr<Enviroment> ancestor(int distance);

    std::any get_at(int distance, const std::string & name);

    void assign_at(int distance, const Token &name, const std::any &value);

private:
    std::unordered_map<std::string, std::any> values;
    std::shared_ptr<Enviroment> enclosing = nullptr;
};



#endif //ENVIROMENT_H
