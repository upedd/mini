#include "Module.h"

void Module::write(OpCode code) {
    write(static_cast<uint8_t>(code));
}

void Module::write(uint8_t data) {
    code.push_back(data);
}

void Module::write(int64_t integer) {
    write(static_cast<uint8_t>(integer >> 56 & 0xFF));
    write(static_cast<uint8_t>(integer >> 48 & 0xFF));
    write(static_cast<uint8_t>(integer >> 40 & 0xFF));
    write(static_cast<uint8_t>(integer >> 32 & 0xFF));
    write(static_cast<uint8_t>(integer >> 24 & 0xFF));
    write(static_cast<uint8_t>(integer >> 16 & 0xFF));
    write(static_cast<uint8_t>(integer >> 8 & 0xFF));
    write(static_cast<uint8_t>(integer & 0xFF));
}

const std::vector<uint8_t> & Module::get_code() {
    return code;
}
