#ifndef MODULEREADER_H
#define MODULEREADER_H
#include "Module.h"

// Module must be kept alive for entire ModuleReader lifetime!
class ModuleReader {
public:
    explicit ModuleReader(const Module& module) : module(module) {};
    uint8_t read();
    OpCode opcode();
    int64_t integer();
    [[nodiscard]] bool at_end() const;
    void add_offset(int offset);

    Value get_constant(int8_t int8);

private:
    int offset = 0;
    const Module& module;
};



#endif //MODULEREADER_H
