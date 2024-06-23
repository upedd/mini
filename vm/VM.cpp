//
// Created by MiłoszK on 23.06.2024.
//
#include "VM.h"

#include <iostream>
using namespace vm;


VM::InterpretResult VM::interpret(Chunk* chunk) {
    this->chunk = chunk;
    this->instruction_ptr = 0;

    while (true) {

        #ifdef VM_TRACE
        std::cout << "          ";
        for (const auto& e : stack) {
            std::cout << '[' << e << ']';
        }
        std::cout << '\n';
        #endif
        Instruction::OpCode instruction { chunk->get_code()[instruction_ptr++] };
        switch (instruction) {
            case Instruction::OpCode::CONSTANT: {
                Value constant = chunk->get_constants()[chunk->get_code()[instruction_ptr++]];
                stack.push_back(constant);
                break;
            }
            case Instruction::OpCode::NEGATE: {
                auto top = stack.back();
                stack.pop_back();
                stack.push_back(-top);
                break;
            }
            case Instruction::OpCode::ADD: {
                double b = stack.back(); stack.pop_back();
                double a = stack.back(); stack.pop_back();
                stack.push_back(a + b);
                break;
            }
            case Instruction::OpCode::SUBTRACT: {
                double b = stack.back(); stack.pop_back();
                double a = stack.back(); stack.pop_back();
                stack.push_back(a - b);
                break;
            }
            case Instruction::OpCode::MULTIPLY: {
                double b = stack.back(); stack.pop_back();
                double a = stack.back(); stack.pop_back();
                stack.push_back(a * b);
                break;
            }
            case Instruction::OpCode::DIVIDE: {
                double b = stack.back(); stack.pop_back();
                double a = stack.back(); stack.pop_back();
                stack.push_back(a / b);
                break;
            }
            case Instruction::OpCode::RETURN: {
                std::cout << stack.back() << '\n';
                stack.pop_back();
                return InterpretResult::OK;
            }
        }
    }
}