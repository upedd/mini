cmake_minimum_required(VERSION 3.22)
project(mini)

set(CMAKE_CXX_STANDARD 23)

add_executable(bite source/main.cpp
        source/main.cpp
        source/parser/Lexer.cpp
        source/parser/Lexer.h
        source/parser/Token.h
        source/common.h
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
        source/types.h
        source/conversions.h
        source/conversions.cpp
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
        source/shared/Message.h
        source/api/enhance_messages.h
        source/base/stream.h
        source/base/box.h
)

target_link_libraries(bite "-lstdc++exp")