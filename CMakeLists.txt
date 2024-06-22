cmake_minimum_required(VERSION 3.28)
project(mini)

set(CMAKE_CXX_STANDARD 23)

add_executable(mini minilang/main.cpp
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
)

add_executable(generate_mini_ast tools/GenerateMiniAst.cpp)
