//
// Created by MiłoszK on 23.06.2024.
//
#include "VM.h"

#include <iostream>
using namespace vm;


void VM::runtime_error(std::string_view message) {
    int line = chunk->get_lines()[instruction_ptr];
    std::cerr << message << '\n';
    std::cerr << "[line " << line << "] in script\n";
}

bool VM::is_falsey(Value top) {
    return top.is_nil() || (top.is_bool() && !top.as_bool());
}

bool VM::values_equal(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case Value::Type::BOOL: return a.as_bool() == b.as_bool();
        case Value::Type::NIL: return true;
        case Value::Type::NUMBER: return a.as_number() == b.as_number();
        case Value::Type::OBJECT: return a.as_object() == b.as_object();
        return false;
    }
}

ObjectString * VM::allocate_string(const std::string &string) {
    if (strings.contains(string)) {
        return strings[string];
    }
    auto* ptr = ObjectString::allocate(string);
    strings[string] = ptr;
    objects.push_back(ptr);
    return ptr;
}

VM::InterpretResult VM::interpret(Chunk* chunk) {
    this->chunk = chunk;
    this->instruction_ptr = 0;

    while (true) {
//#define VM_TRACE
        #ifdef VM_TRACE
        std::cout << "          ";
        for (const auto& e : stack) {
            std::cout << '[' << e.to_string() << ']';
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
            case Instruction::OpCode::NIL: {
                stack.push_back(Value::make_nil());
                break;
            }
            case Instruction::OpCode::TRUE: {
                stack.push_back(Value::make_bool(true));
                break;
            }
            case Instruction::OpCode::FALSE: {
                stack.push_back(Value::make_bool(false));
                break;
            }
            case Instruction::OpCode::NEGATE: {
                auto top = stack.back();
                if (!top.is_number()) {
                    runtime_error("Operand must be a number.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                stack.pop_back();
                stack.push_back(Value::make_number(-top.as_number()));
                break;
            }
            case Instruction::OpCode::ADD: {
                if (stack[stack.size() - 1].is_string() && stack[stack.size() - 2].is_string()) {
                    ObjectString* b = stack.back().as_string(); stack.pop_back();
                    ObjectString* a = stack.back().as_string(); stack.pop_back();
                    stack.push_back(Value::make_object(allocate_string(a->string + b->string)));
                } else if (stack[stack.size() - 1].is_number() && stack[stack.size() - 2].is_number()) {
                    double b = stack.back().as_number(); stack.pop_back();
                    double a = stack.back().as_number(); stack.pop_back();
                    stack.push_back(Value::make_number(a + b));
                } else {
                    runtime_error("Operands must be two numbers or two strings.");
                    return InterpretResult::RUNTIME_ERROR;
                }

                break;
            }
            case Instruction::OpCode::SUBTRACT: {
                if (!stack[stack.size() - 1].is_number() || !stack[stack.size() - 2].is_number()) {
                    runtime_error("Operands must be numbers.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                double b = stack.back().as_number(); stack.pop_back();
                double a = stack.back().as_number(); stack.pop_back();
                stack.push_back(Value::make_number(a - b));
                break;
            }
            case Instruction::OpCode::MULTIPLY: {
                if (!stack[stack.size() - 1].is_number() || !stack[stack.size() - 2].is_number()) {
                    runtime_error("Operands must be numbers.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                double b = stack.back().as_number(); stack.pop_back();
                double a = stack.back().as_number(); stack.pop_back();
                stack.push_back(Value::make_number(a * b));
                break;
            }
            case Instruction::OpCode::DIVIDE: {
                if (!stack[stack.size() - 1].is_number() || !stack[stack.size() - 2].is_number()) {
                    runtime_error("Operands must be numbers.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                double b = stack.back().as_number(); stack.pop_back();
                double a = stack.back().as_number(); stack.pop_back();
                stack.push_back(Value::make_number(a / b));
                break;
            }
            case Instruction::OpCode::NOT: {
                auto top = stack.back(); stack.pop_back();
                stack.push_back(Value::make_bool(is_falsey(top)));
                break;
            }
            case Instruction::OpCode::EQUAL: {
                auto a = stack.back(); stack.pop_back();
                auto b = stack.back(); stack.pop_back();
                stack.push_back(Value::make_bool(values_equal(a, b)));
                break;
            }
            case Instruction::OpCode::GREATER: {
                if (!stack[stack.size() - 1].is_number() || !stack[stack.size() - 2].is_number()) {
                    runtime_error("Operands must be numbers.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                double b = stack.back().as_number(); stack.pop_back();
                double a = stack.back().as_number(); stack.pop_back();
                stack.push_back(Value::make_bool(a > b));
                break;
            }
            case Instruction::OpCode::LESS: {
                if (!stack[stack.size() - 1].is_number() || !stack[stack.size() - 2].is_number()) {
                    runtime_error("Operands must be numbers.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                double b = stack.back().as_number(); stack.pop_back();
                double a = stack.back().as_number(); stack.pop_back();
                stack.push_back(Value::make_bool(a < b));
                break;
            }
            case Instruction::OpCode::RETURN: {
                return InterpretResult::OK;
            }
            case Instruction::OpCode::PRINT: {
                auto top = stack.back(); stack.pop_back();
                std::cout << top.to_string() << '\n';
                break;
            }
        }
    }
}

VM::~VM() {
    free_objects();
}

void VM::free_objects() {
    for (Object* object : objects) {
        switch(object->type) {
            case Object::Type::STRING: delete static_cast<ObjectString*>(object); // NOLINT(*-pro-type-static-cast-downcast)
        }
    }
}
