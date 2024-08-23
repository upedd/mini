#include "Diagnostics.h"

#include <fstream>
#include <algorithm>

#include "base/debug.h"
#include "base/print.h"
#include "base/unordered_dense.h"

using namespace bite;

// ###
// Note: this code is extremely ugly, printing pretty terminal messages is hard...
// ###

// TODO: support for multiline error messages
// TODO: support for mulitple hints in single line

terminal_color diagnostic_level_color(const DiagnosticLevel level) {
    switch (level) {
        case DiagnosticLevel::WARNING: return terminal_color::yellow;
        case DiagnosticLevel::ERROR: return terminal_color::red;
    }
    BITE_PANIC("diagnostic level out of range");
}

void print_diagnostic_level(const DiagnosticLevel level, std::ostream& output, bool is_terminal) {
    switch (level) {
        case DiagnosticLevel::WARNING: {
            if (!is_terminal) {
                bite::print(output, "warning");
            } else {
                bite::print(foreground(diagnostic_level_color(level)) | emphasis::bold, output, "warning");
            }
        };
        case DiagnosticLevel::ERROR: {
            if (!is_terminal) {
                bite::print(output, "error");
            } else {
                bite::print(foreground(diagnostic_level_color(level)) | emphasis::bold, output, "error");
            }
        };
    }
}

/**
 * Main message f.e. "error: missing semicolon"
 */
void print_diagnostic_message(const Diagnostic& diagnostic, std::ostream& output, bool is_terminal) {
    print_diagnostic_level(diagnostic.level, output, is_terminal);
    if (!is_terminal) {
        bite::println(": {}", diagnostic.message);
    } else {
        bite::println(emphasis::bold, ": {}", diagnostic.message);
    }
};


struct CompiledInlineHint {
    std::int64_t line_number;
    std::string line;
    // start, end - offsets from beginning of line
    std::pair<std::int64_t, std::int64_t> in_line_location;
    std::string message;
    DiagnosticLevel level;
};

struct CompiledInlineHintsFile {
    std::filesystem::path filename;
    std::vector<CompiledInlineHint> hints;
};


CompiledInlineHint compile_inline_hint(const InlineHint& hint) {
    // TODO: optimizations (note: displaying these hints is not performance critical as we probably abort after anyways)
    std::ifstream file(hint.location.file_path);
    BITE_ASSERT(file.is_open());
    std::int64_t file_offset = 0;
    std::string line;
    std::int64_t line_number = 0;
    while (std::getline(file, line)) {
        line_number++;
        file_offset += static_cast<std::int64_t>(line.size());
        if (file_offset >= hint.location.start_offset) {
            std::int64_t in_line_start = file_offset - hint.location.start_offset;
            return CompiledInlineHint {
                    .line_number = line_number,
                    .line = line,
                    .in_line_location = {
                        in_line_start,
                        in_line_start + hint.location.end_offset - hint.location.start_offset
                    },
                    .message = hint.message,
                    .level = hint.level
                };
        }
    }
    BITE_PANIC("diagnostic inline hint compilation failed");
}

void print_hint_path(
    const std::filesystem::path& path,
    const std::optional<CompiledInlineHint>& hint,
    std::ostream& output,
    bool is_terminal
) {
    if (is_terminal) {
        bite::print(foreground(terminal_color::bright_black), output, "┌─> {}", path.generic_string());
        if (hint) {
            bite::print(
                foreground(terminal_color::bright_black),
                output,
                ":{}:{}",
                hint->line_number,
                hint->in_line_location.first + 1
            ); // TODO: check this inline location
        }
    } else {
        bite::print(output, "--> {}", path.generic_string());
        if (hint) {
            bite::print(output, ":{}:{}", hint->line_number, hint->in_line_location.first + 1);
        }
    }
    bite::println(output, "");
}

void print_separator(std::ostream& output, bool is_terminal) {
    if (is_terminal) {
        bite::print(foreground(terminal_color::bright_black), output, "│ ");
    } else {
        bite::print(foreground(terminal_color::bright_black), output, "| ");
    }
}

void print_line_number(
    std::size_t max_line_number_width,
    std::int64_t line_number,
    std::ostream& output,
    bool is_terminal
) {
    if (is_terminal) {
        bite::print(emphasis::bold, output, "{}{} ", line_number, repeated(" ", max_line_number_width - line_number));
    } else {
        bite::print(output, "{}{} ", line_number, repeated(" ", max_line_number_width - line_number));
    }
}

void print_inline_hint(const CompiledInlineHint& hint, std::ostream& output, bool is_terminal) {
    // padding
    bite::print(output, "{} ", repeated(" ", hint.in_line_location.first));
    if (is_terminal) {
        bite::println(
            foreground(diagnostic_level_color(hint.level)),
            output,
            "{} {}",
            repeated("^", hint.in_line_location.second - hint.in_line_location.first),
            hint.message
        );
    } else {
        bite::println(
            output,
            "{} {}",
            repeated("^", hint.in_line_location.second - hint.in_line_location.first),
            hint.message
        );
    }
}

void print_hint_file(CompiledInlineHintsFile& file, std::ostream& output, bool is_terminal) {
    BITE_ASSERT(!file.hints.empty());
    std::ranges::sort(file.hints, {}, &CompiledInlineHint::line_number);
    bool has_single_hint = file.hints.size() == 1;
    std::size_t max_line_number_width = std::to_string(file.hints.back().line_number).size();
    // print padding
    bite::print("{}", repeated(" ", max_line_number_width + 1));
    auto single_hint = has_single_hint ? file.hints[0] : std::optional<CompiledInlineHint>();
    print_hint_path(file.filename, single_hint, output, is_terminal);
    for (auto& hint : file.hints) {
        // empty line padding
        bite::print("{}", repeated(" ", max_line_number_width + 1));
        print_separator(output, is_terminal);
        bite::println(output, "");
        print_line_number(max_line_number_width, hint.line_number, output, is_terminal);
        print_separator(output, is_terminal);
        bite::println("{}", hint.line);
        // empty line padding
        bite::print("{}", repeated(" ", max_line_number_width + 1));
        print_separator(output, is_terminal);
        print_inline_hint(hint, output, is_terminal);
    }
}

void print_diagnostic(const Diagnostic& diagnostic, std::ostream& output, bool is_terminal) {
    print_diagnostic_message(diagnostic, output, is_terminal);
    unordered_dense::map<std::filesystem::path, std::vector<CompiledInlineHint>> compiled_hints;
    for (const auto& inline_hint : diagnostic.inline_hints) {
        compiled_hints[inline_hint.location.file_path].push_back(compile_inline_hint(inline_hint));
    }
    std::vector<CompiledInlineHintsFile> compiled_hint_files;
    for (auto& [file, hints] : compiled_hints) {
        compiled_hint_files.emplace_back(file, std::move(hints));
    }
    for (auto& file : compiled_hint_files) {
        print_hint_file(file, output, is_terminal);
    }
}

void DiagnosticManager::print(std::ostream& output, bool is_terminal) {
    for (const auto& diagnostic : diagnostics) {
        print_diagnostic(diagnostic, output, is_terminal);
    }
}
