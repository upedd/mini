//
// Created by MiłoszK on 22.06.2024.
//

#ifndef MINIFUNCTION_H
#define MINIFUNCTION_H
#include "MiniCallable.h"

class MiniFunction : public MiniCallable {
public:
    explicit MiniFunction(Stmt::Function* declaration) : declaration(declaration) {}

    Stmt::Function* declaration = nullptr;

    std::any call(Interpreter *interpreter, std::vector<std::any> arguments) override;
    int arity() override;
};



#endif //MINIFUNCTION_H
