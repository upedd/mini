#ifndef DEBUG_H
#define DEBUG_H
#include <iostream>

#include "Object.h"

class Disassembler {
public:
    explicit Disassembler(Function& function) : function(function) {}


    void disassemble(const std::string& name);

private:
    void simple_opcode(const std::string& name);
    void constant_inst(const std::string& name);
    void double_constant_inst(const std::string& name);
    void class_inst(const std::string& name);
    void arg_inst(const std::string& name);
    void jump_inst(const std::string& name);
    int offset = 0;
    Function& function;
};

inline void Disassembler::simple_opcode(const std::string& name) {
    std::cout << offset - 1 << ": " << name << '\n';
    //offset += 1;
}

inline void Disassembler::constant_inst(const std::string& name) {
    int x = function.get_program().get_at(offset++);
    std::cout << offset - 2 << ": " << name << ' ' << x << ' ' << function.get_constant(x).to_string() << '\n';
}

inline void Disassembler::double_constant_inst(const std::string& name) {
    int x = function.get_program().get_at(offset++);
    int y = function.get_program().get_at(offset++);
    std::cout << offset - 3 << ": " << name << ' ' << x << ' ' << function.get_constant(x).to_string() << ' ' << y << ' ' << function.get_constant(y).to_string() << '\n';
}

inline void Disassembler::class_inst(const std::string& name) {
    int x = function.get_program().get_at(offset++);
    int y = function.get_program().get_at(offset++);
    std::cout << offset - 3 << ": " << name << ' ' << x << ' ' << function.get_constant(x).to_string() << " private: "
        << (y & 1) << " override: " << (y >> 1 & 1) << " abstract: " << (y >> 2 & 1) << " getter: " << (y >> 3 & 1) << " setter: " << (y >> 4 & 1) << '\n';
}

inline void Disassembler::arg_inst(const std::string& name) {
    int x = function.get_program().get_at(offset++);
    std::cout << offset - 2 << ": " << name << ' ' << x << '\n';
}

inline void Disassembler::jump_inst(const std::string& name) {
    int x = function.get_program().get_at(offset++);
    std::cout << offset - 2 << ": " << name << " to: " << x << ' ' << function.get_jump_destination(x) << '\n';
}


inline void Disassembler::disassemble(const std::string& name) {
    std::cout << "--- " << name << " ---\n";
    auto program = function.get_program();
    while (offset < program.size()) {
        switch (static_cast<OpCode>(program.get_at(offset++))) {
            case OpCode::ADD: simple_opcode("ADD");
                break;
            case OpCode::MULTIPLY: simple_opcode("MULTIPLY");
                break;
            case OpCode::SUBTRACT: simple_opcode("SUBTRACT");
                break;
            case OpCode::DIVIDE: simple_opcode("DIVIDE");
                break;
            case OpCode::NEGATE: simple_opcode("NEGATE");
                break;
            case OpCode::TRUE: simple_opcode("TRUE");
                break;
            case OpCode::FALSE: simple_opcode("FALSE");
                break;
            case OpCode::NIL: simple_opcode("NIL");
                break;
            case OpCode::CONSTANT: constant_inst("CONSTANT");
                break;
            case OpCode::EQUAL: simple_opcode("EQUAL");
                break;
            case OpCode::NOT_EQUAL: simple_opcode("NOT_EQUAL");
                break;
            case OpCode::LESS: simple_opcode("LESS");
                break;
            case OpCode::LESS_EQUAL: simple_opcode("LESS_EQUAL");
                break;
            case OpCode::GREATER: simple_opcode("GREATER");
                break;
            case OpCode::GREATER_EQUAL: simple_opcode("GREATER_EQUAL");
                break;
            case OpCode::LEFT_SHIFT: simple_opcode("LEFT_SHIFT");
                break;
            case OpCode::RIGHT_SHIFT: simple_opcode("RIGHT_SHIFT");
                break;
            case OpCode::BITWISE_AND: simple_opcode("BITWISE_AND");
                break;
            case OpCode::BITWISE_OR: simple_opcode("BITWISE_OR");
                break;
            case OpCode::BITWISE_XOR: simple_opcode("BITWISE_XOR");
                break;
            case OpCode::POP: simple_opcode("POP");
                break;
            case OpCode::GET: arg_inst("GET");
                break;
            case OpCode::SET: arg_inst("SET");
                break;
            case OpCode::JUMP_IF_FALSE: jump_inst("JUMP_IF_FALSE");
                break;
            case OpCode::JUMP: jump_inst("JUMP");
                break;
            case OpCode::JUMP_IF_TRUE: jump_inst("JUMP_IF_TRUE");
                break;
            case OpCode::NOT: simple_opcode("NOT");
                break;
            case OpCode::BINARY_NOT: simple_opcode("BINARY_NOT");
                break;
            case OpCode::MODULO: simple_opcode("MODULO");
                break;
            case OpCode::FLOOR_DIVISON: simple_opcode("FLOOR_DIVISON");
                break;
            case OpCode::CALL: arg_inst("CALL");
                break;
            case OpCode::RETURN: simple_opcode("RETURN");
                break;
            case OpCode::CLOSURE: {
                int constant = program.get_at(offset++);
                auto* func = reinterpret_cast<Function*>(function.get_constant(constant).get<Object*>());
                std::cout << (offset - 1) << ": " << "CLOSURE " << constant << '\n';
                for (int i = 0; i < func->get_upvalue_count(); ++i) {
                    bool is_local = program.get_at(offset++);
                    int index = program.get_at(offset++);
                    std::cout << "      " << (is_local ? "local " : "upvalue ") << index << '\n';
                }
                break;
            }
            case OpCode::CLASS_CLOSURE: {
                int constant = program.get_at(offset++);
                auto* func = reinterpret_cast<Function*>(function.get_constant(constant).get<Object*>());
                std::cout << (offset - 1) << ": " << "CLOSURE " << constant << '\n';
                for (int i = 0; i < func->get_upvalue_count(); ++i) {
                    bool is_local = program.get_at(offset++);
                    int index = program.get_at(offset++);
                    std::cout << "      " << (is_local ? "local " : "upvalue ") << index << '\n';
                }
                break;
            }
            case OpCode::GET_UPVALUE: {
                arg_inst("GET_UPVALUE");
                break;
            }
            case OpCode::SET_UPVALUE: {
                arg_inst("SET_UPVALUE");
                break;
            }
            case OpCode::CLOSE_UPVALUE: {
                simple_opcode("CLOSE_UPVALUE");
                break;
            }
            case OpCode::CLASS: {
                constant_inst("CLASS");
                break;
            }
            case OpCode::GET_PROPERTY: {
                constant_inst("GET_PROPERTY");
                break;
            }
            case OpCode::SET_PROPERTY: {
                constant_inst("SET_PROPERTY");
                break;
            }
            case OpCode::METHOD: {
                class_inst("METHOD");
                break;
            }
            case OpCode::INHERIT: {
                simple_opcode("INHERIT");
                break;
            }
            case OpCode::GET_SUPER: {
                constant_inst("GET_SUPER");
                break;
            }
            case OpCode::GET_NATIVE: {
                constant_inst("GET_NATIVE");
                break;
            }
            case OpCode::FIELD: {
                class_inst("FIELD");
                break;
            }
            case OpCode::THIS: {
                simple_opcode("THIS");
                break;
            }
            case OpCode::CONSTRUCTOR: {
                simple_opcode("CONSTRUCTOR");
                break;
            }
            case OpCode::CALL_SUPER_CONSTRUCTOR: {
                arg_inst("CALL_SUPER_CONSTRUCTOR");
                break;
            }
            case OpCode::ABSTRACT_CLASS: {
                constant_inst("ABSTRACT_CLASS");
                break;
            }
            case OpCode::SET_SUPER: {
                constant_inst("SET_SUPER");
                break;
            }
            case OpCode::TRAIT: {
                constant_inst("TRAIT");
                break;
            }
            case OpCode::TRAIT_METHOD: {
                class_inst("TRAIT_METHOD");
                break;
            }
            case OpCode::GET_TRAIT: {
                class_inst("GET_TRAIT");
                break;
            }
            case OpCode::GET_GLOBAL: {
                constant_inst("GET_GLOBAL");
                break;
            }
            case OpCode::SET_GLOBAL: {
                constant_inst("SET_GLOBAL");
                break;
            }
            case OpCode::IMPORT: {
                double_constant_inst("IMPORT");
                break;
            }
            case OpCode::JUMP_IF_NIL: {
                jump_inst("JUMP_IF_NIL");
                break;
            }
            case OpCode::JUMP_IF_NOT_NIL: {
                jump_inst("JUMP_IF_NOT_NIL");
                break;
            }
            case OpCode::JUMP_IF_NOT_UNDEFINED: {
                jump_inst("JUMP_IF_NOT_UNDEFINED");
                break;
            }
        }
    }
}

#endif //DEBUG_H
