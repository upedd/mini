#ifndef CHUNK_H
#define CHUNK_H
#include <vector>

#include "Instruction.h"
#include "value.h"

namespace vm {
    class Chunk {
    public:
        void write(const Instruction& instruction) {
            code.push_back(static_cast<uint8_t>(instruction.get_op_code()));
            lines.push_back(instruction.get_line());
        }

        void write(uint8_t data, int line) {
            code.push_back(data);
            lines.push_back(line);
        }

        int add_constant(const Value value) {
            constants.push_back(value);
            return constants.size() - 1;
        }

        [[nodiscard]] const std::vector<uint8_t>& get_code() const {
            return code;
        }
        [[nodiscard]] const std::vector<Value>& get_constants() const {
            return constants;
        }

        [[nodiscard]] const std::vector<int>& get_lines() const {
            return lines;
        }
    private:
        std::vector<uint8_t> code;
        std::vector<Value> constants;
        std::vector<int> lines;
    };
}


#endif //CHUNK_H
