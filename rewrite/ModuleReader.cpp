#include "ModuleReader.h"

uint8_t ModuleReader::read() {
    return module.get_at(offset++);
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
    return offset >= module.get_code().size();
}
