#ifndef BITE_DEBUG_H
#define BITE_DEBUG_H

#include <cstdint>

#include "print.h"

namespace bite::detail {
    struct SourceLocation {
        const char* file_name;
        std::uint32_t line_number;
        const char* function_name;
    };
    inline void do_assert(const SourceLocation& location, bool expr, const char* expr_str) {
        if (!expr) {
            println("Assertion failed at {}:{} in {}: {}", location.file_name, location.line_number, location.function_name, expr_str);
            std::abort();
        }
    }
}  // namespace bite::detail

#define BITE_SOURCE_LOCATION bite::detail::SourceLocation {__FILE__, __LINE__, __func__}

# ifdef BITE_ENABLE_ASSERT
#define BITE_ASSERT(Expr) bite::detail::do_assert(BITE_SOURCE_LOCATION, Expr, #Expr)
# else
#define BITE_ASSERT(Expr)
# endif

#endif // BITE_DEBUG_H
