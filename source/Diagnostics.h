//
// add comment what it this
//

#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H
#include <string>
#include <vector>
#include <filesystem>

#include "base/debug.h"
#include "shared/StringTable.h"

namespace bite {
    struct SourceSpan {
        std::int64_t start_offset;
        std::int64_t end_offset;
        StringTable::Handle file_path;

        void merge(const SourceSpan& other) {
            BITE_ASSERT(this->file_path == other.file_path);
            this->start_offset = std::min(this->start_offset, other.start_offset);
            this->end_offset = std::max(this->end_offset, other.end_offset);
        }
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
