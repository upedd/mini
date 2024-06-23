//
// Created by MiłoszK on 22.06.2024.
//

#ifndef MINIFUNCTION_H
#define MINIFUNCTION_H
#include "MiniCallable.h"
class MiniInstance;

class MiniFunction : public MiniCallable {
public:
    explicit MiniFunction(Stmt::Function* declaration, std::shared_ptr<Enviroment> closure, bool is_initializer) : declaration(declaration), closure(std::move(closure)), is_initializer(is_initializer) {}

    Stmt::Function* declaration = nullptr;
    std::shared_ptr<Enviroment> closure;
    bool is_initializer;

    std::any call(Interpreter *interpreter, std::vector<std::any> arguments) override;
    int arity() override;
    MiniFunction* bind(MiniInstance* instance);
};



#endif //MINIFUNCTION_H
