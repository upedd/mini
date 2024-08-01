#ifndef LOGGER_H
#define LOGGER_H
#include "print.h"

// possibly optimizations?

namespace bite {
    class Logger;

    template <typename T>
    struct LogPrinter {
        void operator ()(const T& value, Logger* /*logger*/);
    };

    class Logger {
    public:
        enum class Level : std::uint8_t {
            debug,
            info,
            warn,
            error,
            critical,
            off,
        };

        Logger(std::ostream& ostream, const Level level, const bool is_terminal_output = false) :
            m_is_terminal_output(is_terminal_output),
            log_level(level),
            ostream(ostream) {}

        explicit Logger(std::ostream& ostream, const bool is_terminal_output = false) : Logger(
            ostream,
            Level::info,
            is_terminal_output
        ) {}

        template <typename... Args>
        void log(const Level level, std::format_string<Args...> string, Args&&... args) {
            if (level < log_level) {
                return;
            }
            // refactor?
            if (is_terminal_output()) {
                bite::println(
                    ostream,
                    "{}{} {}",
                    formatted_level_string(level),
                    styled(":", emphasis::bold),
                    std::format(string, std::forward<Args>(args)...)
                );
            } else {
                bite::println(
                    ostream,
                    "{}: {}",
                    formatted_level_string(level),
                    std::format(string, std::forward<Args>(args)...)
                );
            }
        }

        template <typename T> // add concept
        void log(const Level level, const T& value) {
            if (level < log_level) {
                return;
            }
            LogPrinter<T>()(value, this);
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

        [[nodiscard]] std::ostream& raw_ostream() const {
            return ostream;
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

        bool m_is_terminal_output;
        Level log_level;
        std::ostream& ostream; // NOLINT(*-avoid-const-or-ref-data-members)
    };

    inline std::strong_ordering operator<=>(Logger::Level& lhs, Logger::Level& rhs) {
        return static_cast<int>(lhs) <=> static_cast<int>(rhs);
    }

    template <typename T>
    void LogPrinter<T>::operator()(const T& value, Logger* logger) {
        logger->log(Logger::Level::info, "{}", value);
    }
}  // namespace bite

#endif //LOGGER_H
