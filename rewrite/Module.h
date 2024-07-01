#ifndef MODULE_H
#define MODULE_H
#include <cstdint>
#include <vector>

#include "OpCode.h"
#include "Value.h"

// todo: investigate std::byte

class Module {
public:
    void write(OpCode code);
    void write(uint8_t data);
    void write(int64_t integer);
    void patch(int position, uint8_t data);

    int get_code_length();

    int add_string_constant(const std::string& string);
    int add_constant(const Value &value);
    [[nodiscard]] Value get_constant(int index) const;

    [[nodiscard]] const std::vector<uint8_t>& get_code() const;
    [[nodiscard]] uint8_t get_at(int index) const;
private:
    std::vector<uint8_t> code; // optim: could use some more specific ds
    std::vector<Value> constants; // should be fixed size?
    std::vector<std::string> strings;
};



#endif //MODULE_H
