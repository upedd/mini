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

        void constant_instruction(std::string_view name) {
            const uint8_t constant = chunk.get_code()[offset + 1];
            std::cout << name << ' ' << static_cast<int>(constant) << ' ';
            std::cout << chunk.get_constants()[constant].to_string() << '\n';
            offset += 2;
        };

        void byte_instruction(std::string_view name) {
            uint8_t slot = chunk.get_code()[offset + 1];
            std::cout << name << ' ' << slot << '\n';
            offset += 2;
        };

        void jump_instruction(std::string_view name, int sign) {
            uint16_t jump = static_cast<uint16_t>(chunk.get_code()[offset + 1]) << 8 | chunk.get_code()[offset + 2];
            std::cout << name << ' ' << offset << " -> " << offset + 3 + sign * jump << '\n';
            offset += 3;
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
                    constant_instruction("OP_CONSTANT");
                    break;
                case Instruction::OpCode::NIL:
                    simple_instruction("OP_NIL");
                    break;
                case Instruction::OpCode::TRUE:
                    simple_instruction("OP_TRUE");
                    break;
                case Instruction::OpCode::FALSE:
                    simple_instruction("OP_FALSE");
                    break;
                case Instruction::OpCode::NOT:
                    simple_instruction("OP_NOT");
                    break;
                case Instruction::OpCode::EQUAL:
                    simple_instruction("OP_EQUAL");
                    break;
                case Instruction::OpCode::GREATER:
                    simple_instruction("OP_GREATER");
                    break;
                case Instruction::OpCode::LESS:
                    simple_instruction("OP_LESS");
                    break;
                case Instruction::OpCode::PRINT:
                    simple_instruction("OP_PRINT");
                    break;
                case Instruction::OpCode::POP:
                    simple_instruction("OP_POP");
                    break;
                case Instruction::OpCode::DEFINE_GLOBAL:
                    constant_instruction("OP_DEFINE_GLOBAL");
                    break;
                case Instruction::OpCode::GET_GLOBAL:
                    constant_instruction("OP_GET_GLOBAL");
                    break;
                case Instruction::OpCode::SET_GLOBAL:
                    constant_instruction("OP_SET_GLOBAL");
                    break;
                case Instruction::OpCode::GET_LOCAL:
                    byte_instruction("OP_GET_LOCAL");
                    break;
                case Instruction::OpCode::SET_LOCAL: {
                    byte_instruction("OP_SET_LOCAL");
                    break;
                }
                case Instruction::OpCode::JUMP_IF_FALSE: {
                    jump_instruction("OP_JUMP_IF_FALSE", 1);
                    break;
                }
                case Instruction::OpCode::JUMP: {
                    jump_instruction("OP_JUMP", 1);
                    break;
                }

                default:
                    std::cout << "Unknown opcode " << static_cast<uint8_t>(op_code) << '\n';
                    offset += 1;
            }
        }
    };
}

#endif //DEBUG_H
