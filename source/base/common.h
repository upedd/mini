#pragma once

// Common macros
#if defined(__GNUC__)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#    define BITE_DENSE_PACK(decl) decl __attribute__((__packed__))
#elif defined(_MSC_VER)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#    define BITE_DENSE_PACK(decl) __pragma(pack(push, 1)) decl __pragma(pack(pop))
#endif

// clang accepts either?
#ifdef _MSC_VER
#    define BITE_NOINLINE [[msvc::noinline]]
#else
#    define BITE_NOINLINE [[gnu::noinline]]
#endif
