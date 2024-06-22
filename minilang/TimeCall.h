//
// Created by MiłoszK on 22.06.2024.
//

#ifndef TIMECALL_H
#define TIMECALL_H
#include "MiniCallable.h"


class TimeCall : public MiniCallable {
public:
    int arity() override;

    std::any call(Interpreter *interpreter, std::vector<std::any> arguments) override;
};


#endif //TIMECALL_H
