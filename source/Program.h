#ifndef PROGRAM_H
#define PROGRAM_H
#include <cstdint>
#include <vector>

#include "OpCode.h"
#include "types.h"

class Program {
public:
    void write(OpCode op_code);
    void write(bite_byte byte);

    void patch(int position, bite_byte byte);

    [[nodiscard]] std::size_t size() const;

    uint8_t get_at(int idx);

private:
    std::vector<bite_byte> code;
};


#endif //PROGRAM_H
