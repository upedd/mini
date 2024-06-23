#include <cassert>
#include <iostream>

#define VM_TRACE

#include "minilang/Mini.h"
#include "vm/Chunk.h"
#include "vm/debug.h"
#include "vm/VM.h"


int main(int argc, char** argv) {
    vm::VM machine;
    vm::Chunk chunk;

    int constant = chunk.add_constant(1.2);
    chunk.write(vm::Instruction(vm::Instruction::OpCode::CONSTANT, 123));
    chunk.write(constant);

    constant = chunk.add_constant( 3.4);
    chunk.write(vm::Instruction(vm::Instruction::OpCode::CONSTANT, 123));
    chunk.write(constant);

    chunk.write(vm::Instruction(vm::Instruction::OpCode::ADD, 123));

    constant = chunk.add_constant( 5.6);
    chunk.write(vm::Instruction(vm::Instruction::OpCode::CONSTANT, 123));
    chunk.write(constant);

    chunk.write(vm::Instruction(vm::Instruction::OpCode::DIVIDE, 123));

    chunk.write(vm::Instruction(vm::Instruction::OpCode::NEGATE, 123));
    chunk.write(vm::Instruction(vm::Instruction::OpCode::RETURN, 123));

    for (auto& v : chunk.get_code()) {
        std::cout << static_cast<int>(v) << ' ';
    }

    vm::debug::Disassembler disassembler("test chunk", chunk);
    disassembler.disassemble();
    machine.interpret(&chunk);
    // assert(argc <= 2);
    // Mini mini;
    // if (argc == 2) {
    //     mini.run_file(argv[1]);
    // } else {
    //     mini.run_prompt();
    // }
    return 0;
}
