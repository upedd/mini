//
// add comment what it this
//

#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H
#include <string>
#include <vector>
#include <filesystem>

namespace bite {
    struct SourceSpan {
        std::int64_t start_offset;
        std::int64_t end_offset;
        std::filesystem::path file_path;
    };

    struct InlineHint {
        SourceSpan location;
        std::string message;
    };

    struct Diagnostic {
        enum class Level {
            WARNING,
            ERROR,
        };
        Level level;
        std::string message;
        std::vector<InlineHint> inline_hints;
    };

    class DiagnosticManager {
    public:
        void add(const Diagnostic& diagnostic) {
            diagnostics.push_back(diagnostic);
        }
        void print(std::ostream& output, bool is_terminal = false);
    private:
        std::vector<Diagnostic> diagnostics;
    };
}

#endif //DIAGNOSTICS_H
