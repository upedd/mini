cmake_minimum_required(VERSION 3.22)
project(mini)

set(CMAKE_CXX_STANDARD 23)

add_executable(bite source/main.cpp
        source/main.cpp
        source/Lexer.cpp
        source/Lexer.h
        source/Token.h
        source/common.h
        source/Parser.cpp
        source/Parser.h
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
        source/parser/perfect_map.h
        source/shared/StringTable.h
        source/shared/SharedContext.h
        source/base/unordered_dense.h
        source/base/common.h
)