//
// Created by MiłoszK on 23.06.2024.
//

#ifndef VM_H
#define VM_H
#include <array>
#include <utility>

#include "Chunk.h"


namespace vm {

    inline constexpr int STACK_MAX = 256;

    class VM {
    public:
        enum class InterpretResult {
            OK,
            COMPILE_ERROR,
            RUNTIME_ERROR
        };

        InterpretResult interpret(Chunk* chunk);
    private:
        Chunk* chunk = nullptr;
        int instruction_ptr = 0;
        std::vector<Value> stack;
    };


}


#endif //VM_H