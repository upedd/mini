#ifndef LOGGER_H
#define LOGGER_H
#include "print.h"

// possibly optimizations?

namespace bite {
    class Logger {
    public:
        enum class Level {
            debug,
            info,
            warn,
            error,
            critical,
            off,
        };

        std::strong_ordering operator<=>(Level& lhs, Level& rhs) const {
            return static_cast<int>(lhs) <=> static_cast<int>(rhs);
        }


        Logger(std::ostream& ostream, const Level level, const bool is_terminal_output = false) :
            m_is_terminal_output(is_terminal_output),
            log_level(level),
            ostream(ostream) {}

        explicit Logger(std::ostream& ostream, const bool is_terminal_output = false) : Logger(ostream, Level::info, is_terminal_output) {}

        template <typename... Args>
        void log(const Level level, std::format_string<Args...> string, Args&&... args) {
            if (level < log_level) return;
            bite::println(
                ostream,
                "{}: {}",
                formatted_level_string(level),
                std::format(string, std::forward<Args>(args)...)
            );
        }

        [[nodiscard]] bool is_terminal_output() const {
            return m_is_terminal_output;
        }

        void set_log_level(const Level& level) {
            this->log_level = level;
        }

        [[nodiscard]] Level level() const {
            return log_level;
        }
    private:
        [[nodiscard]] std::string formatted_level_string(const Level level) const {
            if (m_is_terminal_output) {
                return std::format("{}", styled(level_string(level), foreground(level_color(level)) | emphasis::bold));
            }
            return level_string(level);
        }

        static std::string level_string(const Level level) {
            switch (level) {
                case Level::debug: return "debug";
                case Level::info: return "info";
                case Level::warn: return "warning";
                case Level::error: return "error";
                case Level::critical: return "crtical";
                default: std::unreachable();
            }
        }

        static terminal_color level_color(const Level level) {
            switch (level) {
                case Level::debug: return terminal_color::bright_white;
                case Level::info: return terminal_color::cyan;
                case Level::warn: return terminal_color::yellow;
                case Level::error: return terminal_color::red;
                case Level::critical: return terminal_color::magenta;
                default: std::unreachable();
            }
        }

        bool m_is_terminal_output;
        Level log_level;
        std::ostream& ostream;
    };

    template<typename T>
    struct LogFormatter {
        void operator ()(const T&, const Logger* logger) {

        }
    };
}

#endif //LOGGER_H
