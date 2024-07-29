#ifndef INPUTSTREAM_H
#define INPUTSTREAM_H
#include <fstream>

class InputStreamBase {
public:
    template <class Self>
    bool match(this Self&& self, char c) {
        if (self.peek() == c) {
            self.advance();
            return true;
        }
        return false;
    }
};

// TODO: check errors!
class FileInputStream : public InputStreamBase {
public:
    [[nodiscard]] explicit FileInputStream(const std::string& path) : file(path) {}

    [[nodiscard]] explicit FileInputStream(std::ifstream&& stream) : file(std::move(stream)) {}

    char advance() {
        const int output = file.get();
        if (output == std::ifstream::traits_type::eof()) {
            return '\0';
        }
        ++pos;
        cur = static_cast<char>(output);
        return cur;
    }

    [[nodiscard]] bool at_end() const {
        return file.eof();
    }

    [[nodiscard]] char current() const {
        if (at_end())
            return '\0';
        return cur;
    }

    [[nodiscard]] std::size_t position() const {
        return pos;
    }

    char peek() {
        const int output = file.peek();
        if (output == std::ifstream::traits_type::eof()) {
            return '\0';
        }
        return static_cast<char>(output);
    }

private:
    std::size_t pos = 0;
    char cur = '\0';
    std::ifstream file;
};

#endif //INPUTSTREAM_H
