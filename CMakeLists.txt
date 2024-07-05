cmake_minimum_required(VERSION 3.28)
project(mini)

set(CMAKE_CXX_STANDARD 23)

add_executable(rewrite source/main.cpp
        source/main.cpp
        source/Lexer.cpp
        source/Lexer.h
        source/Token.h
        source/common.h
        source/Parser.cpp
        source/Parser.h
        source/Expr.h
        source/Compiler.cpp
        source/Compiler.h
        source/OpCode.h
        source/VM.cpp
        source/VM.h
        source/Value.h
        source/Value.cpp
        source/Stmt.h
        source/Function.h
        source/CallFrame.h
        source/types.h
        source/conversions.h
        source/conversions.cpp
        source/Program.cpp
        source/Program.h
        source/Function.cpp
        source/debug.h
)