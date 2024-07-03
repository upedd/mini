#ifndef PROGRAM_H
#define PROGRAM_H
#include <vector>

#include "OpCode.h"
#include "types.h"


class Program {
public:
    void write(OpCode op_code);
    void write(bite_byte byte);

    void patch(int position, bite_byte byte);

    [[nodiscard]] std::size_t size() const;
private:
    std::vector<bite_byte> code;
};



#endif //PROGRAM_H
