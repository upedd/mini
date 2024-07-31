#ifndef PRINT_H
#define PRINT_H
#include <cassert>
#include <cstdint>
#include <format>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

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
                is_foreground_color_set = true;
                foreground_color = color;
            } else {
                is_background_color_set = true;
                background_color = color;
            }
        }

        constexpr terminal_style& operator|=(const terminal_style& rhs) {
            if (rhs.is_background_color_set) {
                if (this->is_background_color_set) {
                    assert(false); // TODO: better error handling
                }
                this->background_color = rhs.background_color;
                this->is_background_color_set = true;
            }

            if (rhs.is_foreground_color_set) {
                if (this->is_foreground_color_set) {
                    assert(false); // TODO: better error handling
                }
                this->foreground_color = rhs.foreground_color;
                this->is_foreground_color_set = true;
            }

            this->emphasis |= rhs.emphasis;

            return *this;
        }

        constexpr terminal_style operator|(const terminal_style rhs) {
            return *this |= rhs;
        }

        [[nodiscard]] constexpr terminal_color get_background_color() const { return background_color; }
        [[nodiscard]] constexpr terminal_color get_foreground_color() const { return foreground_color; }
        [[nodiscard]] constexpr bool has_background_color() const { return is_background_color_set; }
        [[nodiscard]] constexpr bool has_foreground_color() const { return is_foreground_color_set; }
        [[nodiscard]] constexpr uint8_t get_emphasis() const { return emphasis; }

    private:
        terminal_color background_color {};
        terminal_color foreground_color {};
        bool is_background_color_set = false;
        bool is_foreground_color_set = false;
        uint8_t emphasis = 0;
    };

    constexpr terminal_style foreground(terminal_color color) {
        return { true, color };
    }

    constexpr terminal_style background(terminal_color color) {
        return { false, color };
    }

    namespace detail {
        constexpr std::uint8_t map_emphasis_to_escape(const emphasis& em) {
            switch (em) {
                case emphasis::bold: return 1;
                case emphasis::faint: return 2;
                case emphasis::italic: return 3;
                case emphasis::underline: return 4;
                case emphasis::blink: return 5;
                case emphasis::reverse: return 7;
                case emphasis::conceal: return 8;
                case emphasis::strikethrough: return 9;
            }
            std::unreachable();
        }

        constexpr std::string get_ansi_escape(const terminal_style& style) {
            std::vector<std::uint8_t> parameters;
            if (style.has_foreground_color()) {
                parameters.push_back(static_cast<std::uint8_t>(style.get_foreground_color()));
            }
            if (style.has_background_color()) {
                parameters.push_back(static_cast<std::uint8_t>(style.get_background_color()) + 10);
            }
            for (std::uint8_t i = 0; i < 8; ++i) {
                if (style.get_emphasis() & 1 << i) {
                    parameters.push_back(map_emphasis_to_escape(static_cast<emphasis>(1 << i)));
                }
            }

            // styling: fix weird formatting
            std::string parameters_string = parameters | std::views::transform(
                [](const uint8_t num) { return std::to_string(num); }
            ) | std::views::join_with(';') | std::ranges::to<std::string>();

            return "\033[" + parameters_string + 'm';
        }

        template <typename T>
        struct StyledArg {
            const T& value;
            terminal_style style;
        };
    }


    // Helpers for printing directly into cout
    // These should be in std but are missing? https://en.cppreference.com/w/cpp/io/print
    // for completness sake
    template <typename... Args>
    void print(std::ostream& ostream, std::format_string<Args...> string, Args&&... args) {
        std::print(ostream, string, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void print(std::format_string<Args...> string, Args&&... args) {
        bite::print(std::cout, string, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void println(std::ostream& ostream, std::format_string<Args...> string, Args&&... args) {
        bite::print(ostream, "{}\n", std::format(string, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void println(std::format_string<Args...> string, Args&&... args) {
        bite::println(std::cout, string, std::forward<Args>(args)...);
    }

    // Repeated printing helper
    template <typename... Args>
    void print_repeat(
        const std::size_t times,
        std::ostream& ostream,
        std::format_string<Args...> string,
        Args&&... args
    ) {
        for (std::size_t i = 0; i < times; ++i) {
            bite::print(ostream, string, std::forward<Args>(args)...);
        }
    }

    template <typename... Args>
    void print_repeat(const std::size_t times, std::format_string<Args...> string, Args&&... args) {
        bite::print_repeat(times, std::cout, string, std::forward<Args>(args)...);
    }

    // ===== Styled printing helpers
    template <typename... Args>
    void print(const terminal_style& style, std::ostream& ostream, std::format_string<Args...> string, Args&&... args) {
        bite::print(
            ostream,
            "{}{}\033[0m",
            detail::get_ansi_escape(style),
            std::format(string, std::forward<Args>(args)...)
        );
    }

    template <typename... Args>
    void print(const terminal_style& style, std::format_string<Args...> string, Args&&... args) {
        bite::print(style, std::cout, string, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void println(
        const terminal_style& style,
        std::ostream& ostream,
        std::format_string<Args...> string,
        Args&&... args
    ) {
        bite::print(style, ostream, "{}\n", std::format(string, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void println(const terminal_style& style, std::format_string<Args...> string, Args&&... args) {
        bite::println(style, std::cout, string, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void print_repeat(
        const terminal_style& style,
        const std::size_t times,
        std::ostream& ostream,
        std::format_string<Args...> string,
        Args&&... args
    ) {
        // optim: print ansi escape before printing these chars
        for (std::size_t i = 0; i < times; ++i) {
            bite::print(style, ostream, string, std::forward<Args>(args)...);
        }
    }

    template <typename... Args>
    void print_repeat(
        const terminal_style& style,
        const std::size_t times,
        std::format_string<Args...> string,
        Args&&... args
    ) {
        bite::print_repeat(style, times, std::cout, string, std::forward<Args>(args)...);
    }
}

template <typename T, typename Char>
struct std::formatter<bite::detail::StyledArg<T>, Char> : std::formatter<T, Char> { // NOLINT(*-dcl58-cpp)
    auto format(const bite::detail::StyledArg<T>& arg, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}{}\033[0m", bite::detail::get_ansi_escape(arg.style), arg.value);
    }
};

namespace bite {
    template <typename T>
    constexpr auto styled(const T& value, terminal_style style) {
        return detail::StyledArg<std::remove_cvref_t<T>>(value, style);
    }
}
#endif //PRINT_H
