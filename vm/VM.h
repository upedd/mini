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

        void runtime_error(std::string_view message);

        static bool is_falsey(Value top);

        bool values_equal(Value a, Value b);

        ObjectString* allocate_string(const std::string& string);

        InterpretResult interpret(Chunk* chunk);

        ~VM();
    private:
        void free_objects();

        Chunk* chunk = nullptr;
        int instruction_ptr = 0;
        std::vector<Value> stack;
        std::vector<Object*> objects;
    };


}


#endif //VM_H
