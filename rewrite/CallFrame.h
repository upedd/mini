#ifndef CALLFRAME_H
#define CALLFRAME_H
#include "Function.h"

struct CallFrame {
    Closure* closure = nullptr;
    int instruction_pointer = 0;
    int frame_pointer = 0;
};

#endif //CALLFRAME_H
