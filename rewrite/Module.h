#ifndef MODULE_H
#define MODULE_H
#include <cstdint>
#include <vector>

#include "OpCode.h"

// todo: investigate std::byte

class Module {
public:
    void write(OpCode code);
    void write(uint8_t data);
    void write(int64_t integer);

    [[nodiscard]] const std::vector<uint8_t>& get_code() const;
    [[nodiscard]] uint8_t get_at(int index) const;
private:
    std::vector<uint8_t> code;
};



#endif //MODULE_H
