#include <cassert>
#include <iostream>

#include "minilang/Mini.h"
#include "vm/Chunk.h"
#include "vm/debug.h"

int main(int argc, char** argv) {
    vm::Chunk chunk;

    int constant = chunk.add_constant(1.2);
    chunk.write(vm::Instruction(vm::Instruction::OpCode::CONSTANT, 123));
    chunk.write(constant);
    chunk.write(vm::Instruction(vm::Instruction::OpCode::RETURN, 123));
    vm::debug::Disassembler disassembler("test chunk", chunk);
    disassembler.disassemble();

    // assert(argc <= 2);
    // Mini mini;
    // if (argc == 2) {
    //     mini.run_file(argv[1]);
    // } else {
    //     mini.run_prompt();
    // }
    return 0;
}
