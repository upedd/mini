#include "ModuleReader.h"

uint8_t ModuleReader::read() {
    return frame->function->code.get_at(frame->instruction_pointer++);
}

OpCode ModuleReader::opcode() {
    return static_cast<OpCode>(read());
}

int64_t ModuleReader::integer() {
    int64_t output = static_cast<int64_t>(read()) << 56;
    output |= static_cast<int64_t>(read()) << 48;
    output |= static_cast<int64_t>(read()) << 40;
    output |= static_cast<int64_t>(read()) << 32;
    output |= static_cast<int64_t>(read()) << 24;
    output |= static_cast<int64_t>(read()) << 16;
    output |= static_cast<int64_t>(read()) << 8;
    output |= static_cast<int64_t>(read());
    return output;
}

bool ModuleReader::at_end() const {
    return frame->instruction_pointer >= frame->function->code.get_code().size();
}

void ModuleReader::add_offset(int offset) {
    frame->instruction_pointer += offset;
}

Value ModuleReader::get_constant(int8_t int8) {
    return frame->function->code.get_constant(int8);
}

void ModuleReader::set_frame(CallFrame &value) {
    frame = &value;
}
