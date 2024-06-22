#ifndef MINI_H
#define MINI_H
#include <string_view>

#include "Interpreter.h"
#include "RuntimeError.h"
#include "Token.h"

class Mini {
public:
    void run(std::string_view input);
    void run_prompt();
    void run_file(std::string_view filename);

    static void runtime_error(const RuntimeError & error);
    static void error(int line, std::string_view message);
    static void error(const Token& token, std::string_view message);
private:
    static void report(int line, std::string_view where, std::string_view message);
    inline static bool had_error = false;
    inline static bool had_runtime_error = false;
    Interpreter interpreter;
};

#endif //MINI_H
