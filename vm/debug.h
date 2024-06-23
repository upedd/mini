//
// Created by MiłoszK on 23.06.2024.
//

#ifndef DEBUG_H
#define DEBUG_H
#include <format>
#include <iostream>
#include <string_view>
#include <utility>

#include "Chunk.h"

namespace vm::debug {
    class Disassembler {
    public:
        explicit Disassembler(const std::string_view name, Chunk chunk) : name(name), chunk(std::move(chunk)) {
        }

        void disassemble() {
            std::cout << "== " << name << " ==" << '\n';
            while (offset < chunk.get_code().size()) {
                disassemble_instruction();
            }
        }

    private:
        Chunk chunk;
        const std::string name;
        int offset = 0;

        void simple_instruction(const std::string_view name) {
            std::cout << name << '\n';
            offset += 1;
        }

        void constant_instruction() {
            const uint8_t constant = chunk.get_code()[offset + 1];
            std::cout << "OP_CONSTANT " << static_cast<int>(constant) << ' ';
            std::cout << chunk.get_constants()[constant] << '\n';
            offset += 2;
        };

        void disassemble_instruction() {
            std::cout << std::format("{:04} ", offset);

            if (offset > 0 && chunk.get_lines()[offset] == chunk.get_lines()[offset - 1]) {
                std::cout << "   | ";
            } else {
                std::cout << std::format("{:4} ", chunk.get_lines()[offset]);
            }

            auto &code = chunk.get_code();
            Instruction::OpCode op_code {code[offset]};
            switch (op_code) {
                case Instruction::OpCode::RETURN:
                    simple_instruction("OP_RETURN");
                    break;
                case Instruction::OpCode::NEGATE:
                    simple_instruction("OP_NEGATE");
                    break;
                case Instruction::OpCode::ADD:
                    simple_instruction("OP_ADD");
                    break;
                case Instruction::OpCode::SUBTRACT:
                    simple_instruction("OP_SUBTRACT");
                    break;
                case Instruction::OpCode::MULTIPLY:
                    simple_instruction("OP_MULTIPLY");
                    break;
                case Instruction::OpCode::DIVIDE:
                    simple_instruction("OP_DIVIE");
                    break;
                case Instruction::OpCode::CONSTANT:
                    constant_instruction();
                    break;
                default:
                    std::cout << "Unknown opcode " << static_cast<uint8_t>(op_code) << '\n';
                    offset += 1;
            }
        }
    };
}

#endif //DEBUG_H
