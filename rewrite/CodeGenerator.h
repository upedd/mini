#ifndef CODEGENERATOR_H
#define CODEGENERATOR_H
#include "Expr.h"
#include "Module.h"


class CodeGenerator {
public:
    class Error : public std::runtime_error {
    public:
        explicit Error(const std::string& message) : runtime_error(message) {}
    };

    void generate(const Expr &expr);

    Module get_module();
private:
    void literal(const LiteralExpr &expr);
    void unary(const UnaryExpr & expr);
    void binary(const BinaryExpr & expr);
    void string_literal(const StringLiteral& expr);

    Module module;
};



#endif //CODEGENERATOR_H
