#ifndef CALLFRAME_H
#define CALLFRAME_H
#include "Object.h"

struct CallFrame {
    Closure* closure = nullptr;
    int instruction_pointer = 0;
    int frame_pointer = 0;
    std::array<Value, 256> locals; // should be dynamically sized!
};

#endif //CALLFRAME_H
