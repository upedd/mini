#ifndef INPUTSTREAM_H
#define INPUTSTREAM_H
#include <fstream>

class InputStreamBase {
public:
    template<class Self>
    bool match(this Self &&self, char c) {
        if (self.current == c) {
            self.advance();
            return true;
        }
        return false;
    }
};

// TODO: check errors!
class FileInputStream : public InputStreamBase {
public:
    [[nodiscard]] explicit FileInputStream(const std::string &path) : file(path) { // NOLINT(*-pro-type-member-init)
        advance();
    }

    [[nodiscard]] explicit FileInputStream(std::ifstream &&stream) : file(std::move(stream)) { // NOLINT(*-pro-type-member-init)
        advance();
    }

    char advance() {
        int output = file.get();
        if (output == std::ifstream::traits_type::eof()) {
            return '\0';
        }
        cur = static_cast<char>(output);
        return cur;
    }

    [[nodiscard]] bool at_end() const {
        return file.eof();
    }

    [[nodiscard]] char current() const {
        if (at_end()) return '\0';
        return cur;
    }

    int position() {
        return file.tellg();
    }
private:
    char cur;
    std::ifstream file;
};

#endif //INPUTSTREAM_H
