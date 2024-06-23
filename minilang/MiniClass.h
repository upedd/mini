//
// Created by MiłoszK on 23.06.2024.
//

#ifndef MINICLASS_H
#define MINICLASS_H
#include <string>
#include <utility>

#include "MiniCallable.h"
#include "MiniFunction.h"


class MiniClass : public MiniCallable {
public:

    MiniClass(std::string  name, MiniClass* superclass, const std::unordered_map<std::string, MiniFunction *> & methods) : name(std::move(name)), methods(methods), superclass(superclass) {}

    std::string name;
    MiniClass* superclass;
    std::unordered_map<std::string, MiniFunction *> methods;
    std::any call(Interpreter *interpreter, std::vector<std::any> arguments) override;
    int arity() override;

    MiniFunction * find_method(const std::string & string);
};



#endif //MINICLASS_H
