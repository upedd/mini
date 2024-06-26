cmake_minimum_required(VERSION 3.28)
project(mini)

set(CMAKE_CXX_STANDARD 23)

add_executable(mini main.cpp
        minilang/Mini.cpp
        minilang/Mini.h
        minilang/TokenType.h
        minilang/Token.cpp
        minilang/Token.h
        minilang/Scanner.cpp
        minilang/Scanner.h
        minilang/generated/Expr.h
        minilang/AstPriter.cpp
        minilang/AstPrinter.h
        minilang/Parser.cpp
        minilang/Parser.h
        minilang/Interpreter.cpp
        minilang/Interpreter.h
        minilang/RuntimeError.h
        minilang/generated/Stmt.h
        minilang/Enviroment.cpp
        minilang/Enviroment.h
        minilang/MiniCallable.h
        minilang/TimeCall.cpp
        minilang/TimeCall.h
        minilang/MiniFunction.cpp
        minilang/MiniFunction.h
        minilang/Return.h
        minilang/Resolver.cpp
        minilang/Resolver.h
        minilang/MiniClass.cpp
        minilang/MiniClass.h
        minilang/MiniInstance.cpp
        minilang/MiniInstance.h

)

add_executable(generate_mini_ast tools/GenerateMiniAst.cpp)
add_executable(vm_mini
        vm/main.cpp
        vm/Chunk.h
        vm/Instruction.h
        vm/debug.h
        vm/value.h
        vm/VM.h
        vm/VM.cpp
        vm/Compiler.cpp
        vm/Compiler.h
        vm/Scanner.cpp
        vm/Scanner.h
        vm/Token.cpp
        vm/Token.h
        vm/Value.cpp)

add_executable(rewrite rewrite/main.cpp
        rewrite/main.cpp
        rewrite/Lexer.cpp
        rewrite/Lexer.h
        rewrite/Token.h
        rewrite/common.h)