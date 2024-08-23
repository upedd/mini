#include "Diagnostics.h"

#include <fstream>

#include "base/debug.h"
#include "base/print.h"
#include "base/unordered_dense.h"

using namespace bite;

// TODO: support for multiline error messages

void print_diagnostic_level(const Diagnostic::Level level, std::ostream& output, bool is_terminal) {
    switch (level) {
        case Diagnostic::Level::WARNING: {
            if (!is_terminal) {
                bite::print(output, "warning");
            } else {
                bite::print(foreground(terminal_color::yellow) | emphasis::bold, output, "warning");
            }
        };
        case Diagnostic::Level::ERROR: {
            if (!is_terminal) {
                bite::print(output, "error");
            } else {
                bite::print(foreground(terminal_color::red) | emphasis::bold, output, "error");
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
        bite::print(": {}", diagnostic.message);
    } else {
        bite::print(emphasis::bold, ": {}", diagnostic.message);
    }
};


struct CompiledInlineHint {
    std::string line;
    // start, end - offsets from beginning of line
    std::pair<std::int64_t, std::int64_t> in_line_location;
    std::string message;
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
    while (std::getline(file, line)) {
        file_offset += static_cast<std::int64_t>(line.size());
        if (file_offset >= hint.location.start_offset) {
            std::int64_t in_line_start = file_offset - hint.location.start_offset;
            return CompiledInlineHint {
                    .line = line,
                    .in_line_location = {
                        in_line_start,
                        in_line_start + hint.location.end_offset - hint.location.start_offset
                    },
                    .message = hint.message
                };
        }
    }
    BITE_PANIC("diagnostic inline hint compilation failed");
}

void print_hint_file(const CompiledInlineHintsFile& file, std::ostream& output, bool is_terminal) {
    BITE_ASSERT(!file.hints.empty());
    bool has_single_hint = file.hints.size() == 1;

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
