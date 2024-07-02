#ifndef MODULEREADER_H
#define MODULEREADER_H
#include "Module.h"
#include "CallFrame.h"

// Module must be kept alive for entire ModuleReader lifetime!
class ModuleReader {
public:
    uint8_t read();
    OpCode opcode();
    int64_t integer();
    [[nodiscard]] bool at_end() const;
    void add_offset(int offset);

    Value get_constant(int8_t int8);

    void set_frame(CallFrame& value);

private:
    CallFrame* frame = nullptr;
};



#endif //MODULEREADER_H
