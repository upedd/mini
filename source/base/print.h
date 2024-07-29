#ifndef PRINT_H
#define PRINT_H
#include <cassert>
#include <cstdint>
#include <string>

// Common utilites related to std::print

namespace bite {

    // inspired by: https://github.com/fmtlib/fmt/blob/master/include/fmt/color.h

    // https://en.wikipedia.org/wiki/ANSI_escape_code#3-bit_and_4-bit
    enum class terminal_color : std::uint8_t {
        black = 30,
        red,
        green,
        yellow,
        blue,
        magenta,
        cyan,
        white,
        bright_black = 90,
        bright_red,
        bright_green,
        bright_yelow,
        bright_blue,
        bright_magenta,
        bright_cyan,
        bright_white
    };

    enum class emphasis : std::uint8_t {
        bold = 1,
        faint = 1 << 1,
        italic = 1 << 2,
        underline = 1 << 3,
        blink = 1 << 4,
        reverse = 1 << 5,
        conceal = 1 << 6,
        strikethrough = 1 << 7
    };


    class terminal_style {
    public:
        constexpr terminal_style(emphasis em) : emphasis(static_cast<uint8_t>(em)) {}
        constexpr terminal_style(const bool is_foreground, const terminal_color color) {
            if (is_foreground) {
                has_foreground_color = true;
                foreground_color = color;
            } else {
                has_foreground_color = true;
                background_color = color;
            }
        }

        constexpr terminal_style& operator|=(const terminal_style& rhs) {
            if (rhs.has_foreground_color) {
                if (this->has_foreground_color) {
                    assert(false); // TODO: better error handling
                }
                this->foreground_color = rhs.foreground_color;
                this->has_foreground_color = true;
            }

            if (rhs.has_background_color) {
                if (this->has_foreground_color) {
                    assert(false); // TODO: better error handling
                }
                this->background_color = rhs.background_color;
                this->has_background_color = true;
            }

            this->emphasis |= rhs.emphasis;
        }
    private:
        terminal_color background_color {};
        terminal_color foreground_color {};
        bool has_background_color = false;
        bool has_foreground_color = false;
        uint8_t emphasis = 0;
    };

    constexpr terminal_style foreground(terminal_color color) {
        return {true, color};
    }

    constexpr terminal_style background(terminal_color color) {
        return {false, color};
    }

    namespace print::detail {
        constexpr std::string get_ansi_escape(const terminal_style& style) {
            std::string output {""};
        }
    }
}

#endif //PRINT_H
