//
// Created by MiłoszK on 23.06.2024.
//

#ifndef MINICLASS_H
#define MINICLASS_H
#include <string>

#include "MiniCallable.h"
#include "MiniFunction.h"


class MiniClass : public MiniCallable {
public:

    MiniClass(const std::string & name, const std::unordered_map<std::string, MiniFunction *> & methods) : name(name), methods(methods) {}

    std::string name;
    std::unordered_map<std::string, MiniFunction *> methods;
    std::any call(Interpreter *interpreter, std::vector<std::any> arguments) override;
    int arity() override;

    MiniFunction * find_method(const std::string & string);
};



#endif //MINICLASS_H
