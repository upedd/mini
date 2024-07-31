#ifndef STREAM_H
#define STREAM_H

#include <fstream>

// === Common abstraction for streams and file stream
// TODO: off by one
// TODO: encapsulate
// TODO: constructor?
namespace bite {
    template <typename T>
    class input_stream_base {
    public:
        template <class Self>
        T advance(this Self&& self) {
            if (!self.ended()) {
                self.m_current = self.m_next;
                self.m_next = self.get(self.m_position++);
                return self.m_current;
            }
            return self.default_value();
        }

        template <class Self>
        [[nodiscard]] T current(this Self&& self) {
            return self.m_current;
        }

        template <class Self>
        [[nodiscard]] T next(this Self&& self) {
            return self.m_next;
        }

        template <class Self>
        bool match(this Self&& self, T c) {
            if (self.next() == c) {
                self.advance();
                return true;
            }
            return false;
        }

        template <class Self>
        [[nodiscard]] std::size_t position(this Self&& self)  {
            return self.m_position;
        }

        T m_current;
        T m_next;
        std::size_t m_position = 0;
    };

    // TODO: check errors!
    class file_input_stream : public input_stream_base<char> {
    public:
        [[nodiscard]] explicit file_input_stream(const std::string& path) :file(path) {
            advance();
        }

        [[nodiscard]] explicit file_input_stream(std::ifstream&& stream) : file(std::move(stream)) {
            advance();
        }

        [[nodiscard]] bool ended() const {
            return file.eof();
        }

        [[nodiscard]] char get(std::size_t) {
            return static_cast<char>(file.get()); // safe?
        }

        [[nodiscard]] char default_value() const {
            return '\0';
        }

    private:
        std::ifstream file;
    };
}
#endif //STREAM_H
