//
// Created by MiłoszK on 19.06.2024.
//

#ifndef ASTPRITER_H
#define ASTPRITER_H
#include <iostream>

#include "generated/Expr.h"
#include <sstream>


class AstPrinter : public Expr::Visitor {
public:

    static std::string _please_remove_cast_any_to_string(const std::any& any) {
        if (any.type() == typeid(const char*)) {
            return std::any_cast<const char*>(any);
        } else {
            return std::any_cast<std::string>(any);
        }
    }

    std::string print(Expr* expr) {
        // TODO: better way without throwing
        // TODO: there must be a way to do it without shared pointers.
        // TODO: why we sometimes supply std::string to any_cast and other time const char*?
        try {
            auto s = _please_remove_cast_any_to_string(expr->accept(this));
            return s;
        } catch (const std::bad_any_cast& e) {
            return "";
        }
    }

    std::any visitBinaryExpr(Expr::Binary *expr) override {
        return parenthesize(expr->op.lexeme, {expr->left, expr->right});
    }

    std::any visitGroupingExpr(Expr::Grouping *expr) override {
        return parenthesize("group", std::vector { expr->expression });
    }

    std::any visitLiteralExpr(Expr::Literal *expr) override {
        if (!expr->value.has_value()) return "nil";
        return expr->value.type().name();
    }

    std::any visitUnaryExpr(Expr::Unary *expr) override {
        return parenthesize(expr->op.lexeme, {expr->right});
    }

    std::string parenthesize(std::string_view name, const std::vector<std::shared_ptr<Expr>>& exprs) {
        std::stringstream ss;
        ss << "(" << name;
        for (auto& expr : exprs) {
            ss << " " << _please_remove_cast_any_to_string(expr->accept(this));
        }
        ss << ")";
        return ss.str();
    }
};



#endif //ASTPRITER_H
