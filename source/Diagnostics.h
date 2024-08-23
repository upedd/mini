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

    enum class DiagnosticLevel : std::uint8_t {
        INFO,
        WARNING,
        ERROR,
    };

    struct InlineHint {
        SourceSpan location;
        std::string message;
        DiagnosticLevel level;
    };


    struct Diagnostic {
        DiagnosticLevel level;
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
}  // namespace bite

#endif //DIAGNOSTICS_H
