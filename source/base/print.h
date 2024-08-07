#ifndef PRINT_H
#define PRINT_H
#include <cassert>
#include <cstdint>
#include <format>
#include <iostream>
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
        faint = 1U << 1U,
        italic = 1U << 2U,
        underline = 1U << 3U,
        blink = 1U << 4U,
        reverse = 1U << 5U,
        conceal = 1U << 6U,
        strikethrough = 1U << 7U
    };


    class terminal_style {
    public:
        // NOLINT(*-explicit-constructor)
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
                if (style.get_emphasis() & 1U << i) {
                    parameters.push_back(map_emphasis_to_escape(static_cast<emphasis>(1U << i)));
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
            const T& value; // NOLINT(*-avoid-const-or-ref-data-members)
            terminal_style style;
        };

        template <typename T>
        struct RepatedArg {
            const T& value; // NOLINT(*-avoid-const-or-ref-data-members)
            std::size_t times;
        };
    }  // namespace detail


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
}  // namespace bite

template <typename T, typename Char>
struct std::formatter<bite::detail::StyledArg<T>, Char> : std::formatter<T, Char> { // NOLINT(*-dcl58-cpp)
    auto format(const bite::detail::StyledArg<T>& arg, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}{}\033[0m", bite::detail::get_ansi_escape(arg.style), arg.value);
    }
};

template <typename T, typename Char>
struct std::formatter<bite::detail::RepatedArg<T>, Char> : std::formatter<basic_string_view<Char>> { // NOLINT(*-dcl58-cpp)
    auto format(const bite::detail::RepatedArg<T>& arg, std::format_context& ctx) const {
        std::basic_string<Char> temp;
        for (std::size_t i = 0; i < arg.times; ++i) {
            std::format_to(std::back_inserter(temp), "{}", arg.value); // optim?
        }
        return std::formatter<basic_string_view<Char>>::format(temp, ctx);
    }
};


namespace bite {
    template <typename T>
    constexpr auto styled(const T& value, terminal_style style) {
        return detail::StyledArg<std::remove_cvref_t<T>>(value, style);
    }

    template <typename T>
    constexpr auto repeated(const T& value, std::size_t times) {
        return detail::RepatedArg<std::remove_cvref_t<T>>(value, times);
    }
}  // namespace bite
#endif //PRINT_H
