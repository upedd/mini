//
// Created by MiłoszK on 23.06.2024.
//

#ifndef MINICLASS_H
#define MINICLASS_H
#include <string>

#include "MiniCallable.h"


class MiniClass : public MiniCallable {
public:
    MiniClass(std::string_view name) : name(name) {}

    std::string name;

    std::any call(Interpreter *interpreter, std::vector<std::any> arguments) override;
    int arity() override;

};



#endif //MINICLASS_H
