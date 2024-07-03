#include "Program.h"

void Program::write(OpCode op_code) {
    write(static_cast<bite_byte>(op_code));
}

void Program::write(bite_byte byte) {
    code.push_back(byte);
}

void Program::patch(int position, bite_byte byte) {
    code[position] = byte; // range check?
}

std::size_t Program::size() const {
    return code.size();
}
