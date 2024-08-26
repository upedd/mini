cmake_minimum_required(VERSION 3.22)
project(mini)

set(CMAKE_CXX_STANDARD 23)

if (MSVC)
    # warning level 4
    add_compile_options(/W4)
else()
    # additional warnings
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(bite source/main.cpp
        source/main.cpp
        source/parser/Lexer.cpp
        source/parser/Lexer.h
        source/parser/Token.h
        source/parser/Parser.cpp
        source/parser/Parser.h
        source/Ast.h
        source/Compiler.cpp
        source/Compiler.h
        source/OpCode.h
        source/VM.cpp
        source/VM.h
        source/Value.h
        source/Value.cpp
        source/Object.h
        source/CallFrame.h
        source/shared/types.h
        source/parser/conversions.h
        source/parser/conversions.cpp
        source/Program.cpp
        source/Program.h
        source/Object.cpp
        source/debug.h
        source/GarbageCollector.cpp
        source/GarbageCollector.h
        source/base/perfect_map.h
        source/shared/StringTable.h
        source/shared/SharedContext.h
        source/base/unordered_dense.h
        source/base/common.h
        source/base/hash.h
        source/base/print.h
        source/base/logger.h
        source/base/stream.h
        source/base/box.h
        source/base/overloaded.h
        source/base/chars.h
        source/base/bitflags.h
        source/Analyzer.cpp
        source/Analyzer.h
        source/base/debug.h
        source/Diagnostics.h
        source/Diagnostics.cpp
        source/AstVisitor.h
)

# TODO: disable in release builds
target_compile_definitions(bite PRIVATE BITE_ENABLE_ASSERT)
# workaround (or not?) to make std::print work on gcc
target_link_libraries(bite "-lstdc++exp")