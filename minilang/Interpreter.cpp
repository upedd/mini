#include "Interpreter.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <memory>

#include "Mini.h"
#include "MiniCallable.h"
#include "MiniClass.h"
#include "MiniFunction.h"
#include "MiniInstance.h"
#include "Return.h"
#include "TimeCall.h"

Interpreter::Interpreter() {
    // TODO memory leak?
    MiniCallable* ptr = new TimeCall();
    globals->define("time", ptr);
    enviroment = globals;
}

void Interpreter::interpret(const std::vector<std::unique_ptr<Stmt>>& statements) {
    try {
        for (const auto& statement : statements) {
            execute(statement.get());
        }
    } catch (const RuntimeError& error) {
        Mini::runtime_error(error);
    }
}

std::any Interpreter::visitLiteralExpr(Expr::Literal *expr) {
    return expr->value;
}

std::any Interpreter::visitGroupingExpr(Expr::Grouping *expr) {
    return evaluate(expr->expression.get());
}

std::any Interpreter::visitUnaryExpr(Expr::Unary *expr) {
    auto right = evaluate(expr->right.get());
    switch (expr->op.type) {
        case TokenType::BANG:
            return !isTruthy(right);
        case TokenType::MINUS:
            check_number_operand(expr->op, right);
            return -std::any_cast<double>(right);
        default:
            return nullptr;
    }
}

std::any Interpreter::visitBinaryExpr(Expr::Binary *expr) {
    auto left = evaluate(expr->left.get());
    auto right = evaluate(expr->right.get());


    switch (expr->op.type) {
        case TokenType::MINUS:
            check_number_operands(expr->op, left, right);
            return std::any_cast<double>(left) - std::any_cast<double>(right);
        case TokenType::SLASH:
            check_number_operands(expr->op, left, right);
            return std::any_cast<double>(left) / std::any_cast<double>(right);
        case TokenType::STAR:
            check_number_operands(expr->op, left, right);
            return std::any_cast<double>(left) * std::any_cast<double>(right);
        case TokenType::PLUS:
            if (left.type() == typeid(double) && right.type() == typeid(double)) {
                return std::any_cast<double>(left) + std::any_cast<double>(right);
            }
            if (left.type() == typeid(std::string) && right.type() == typeid(std::string)) {
                return std::any_cast<std::string>(left) + std::any_cast<std::string>(right);
            }
            throw RuntimeError(expr->op, "Operands must be two numbers or two strings.");
        case TokenType::GREATER:
            check_number_operands(expr->op, left, right);
            return std::any_cast<double>(left) > std::any_cast<double>(right);
        case TokenType::GREATER_EQUAL:
            check_number_operands(expr->op, left, right);
            return std::any_cast<double>(left) >= std::any_cast<double>(right);
        case TokenType::LESS:
            check_number_operands(expr->op, left, right);
            return std::any_cast<double>(left) < std::any_cast<double>(right);
        case TokenType::LESS_EQUAL:
            check_number_operands(expr->op, left, right);
            return std::any_cast<double>(left) <= std::any_cast<double>(right);
        case TokenType::BANG_EQUAL:
            return !isEqual(left, right);
        case TokenType::EQUAL_EQUAL:
            return isEqual(left, right);
        default:
            break;
    }
    return nullptr;
}

std::any Interpreter::visitExpressionStmt(Stmt::Expression *stmt) {
    evaluate(stmt->expression.get());
    return nullptr;
}

std::any Interpreter::visitPrintStmt(Stmt::Print *stmt) {
    auto value = evaluate(stmt->expression.get());
    std::cout << stringify(value) << '\n';
    return nullptr;
}

std::any Interpreter::visitVarStmt(Stmt::Var *stmt) {
    std::any value = nullptr;
    if (stmt->initializer != nullptr) {
        value = evaluate(stmt->initializer.get());
    }
    enviroment->define(stmt->name.lexeme, value);
    return nullptr;
}

std::any Interpreter::visitClassStmt(Stmt::Class *stmt) {
    enviroment->define(stmt->name.lexeme, {});
    // TODO memory leak
    MiniCallable* klass = new MiniClass(stmt->name.lexeme);
    enviroment->assign(stmt->name, klass);
    return {};
}

std::any Interpreter::look_up_variable(const Token &token, Expr *expr) {
    if (locals.contains(expr)) {
        return enviroment->get_at(locals[expr], token.lexeme);
    }
    return globals->get(token);
}

std::any Interpreter::visitVariableExpr(Expr::Variable *expr) {
    return look_up_variable(expr->name, expr);
}

std::any Interpreter::visitAssignExpr(Expr::Assign *expr) {
    auto value = evaluate(expr->value.get());

    if (locals.contains(expr)) {
        enviroment->assign_at(locals[expr], expr->name, value);
    } else {
        globals->assign(expr->name, value);
    }


    return value;
}

std::any Interpreter::visitBlockStmt(Stmt::Block *stmt) {
    execute_block(stmt->statements, Enviroment(enviroment));
    return nullptr;
}

std::any Interpreter::visitIfStmt(Stmt::If *stmt) {
    if (isTruthy(evaluate(stmt->condition.get()))) {
        execute(stmt->then_branch.get());
    } else if (stmt->else_branch) {
        execute(stmt->else_branch.get());
    }
    return nullptr;
}

std::any Interpreter::visitLogicalExpr(Expr::Logical *expr) {
    auto left = evaluate(expr->left.get());
    if (expr->op.type == TokenType::OR) {
        if (isTruthy(left)) return left;
    } else {
        if (!isTruthy(left)) return left;
    }

    return evaluate(expr->right.get());
}

std::any Interpreter::visitWhileStmt(Stmt::While *stmt) {
    // DEBUG???
    while (isTruthy(evaluate(stmt->condition.get()))) {
        execute(stmt->body.get());
    }
    return nullptr;
}

std::any Interpreter::visitCallExpr(Expr::Call *expr) {
    auto callee = evaluate(expr->callee.get());
    std::vector<std::any> arguments;
    for (auto& argument : expr->arguments) {
        arguments.push_back(evaluate(argument.get()));
    }
    try {
        auto function = std::any_cast<MiniCallable*>(callee);

        if (arguments.size() != function->arity()) {
            throw RuntimeError(expr->paren, "Expected " + std::to_string(function->arity()) + " arguments but got " + std::to_string(arguments.size()) + ".");
        }

        return function->call(this, arguments);
    } catch (const std::bad_any_cast& _) {
        throw RuntimeError(expr->paren, "Can only call functions and classes");
    }
    // TODO
}

std::any Interpreter::visitFunctionStmt(Stmt::Function *stmt) {
    // TODO: memory leak!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    MiniCallable* function = new MiniFunction(stmt, enviroment);
    enviroment->define(stmt->name.lexeme, function);
    return nullptr;
}

std::any Interpreter::visitReturnStmt(Stmt::Return *stmt) {
    std::any value = nullptr;
    if (stmt->value != nullptr) {
        value = evaluate(stmt->value.get());
    }
    throw Return(value);
}

std::any Interpreter::visitGetExpr(Expr::Get *expr) {
    auto object = evaluate(expr->object.get());
    if (object.type() == typeid(MiniInstance*)) {
        return std::any_cast<MiniInstance*>(object)->get(expr->name);
    }
    throw RuntimeError(expr->name ,"Only instances have properties.");
}

std::any Interpreter::visitSetExpr(Expr::Set *expr) {
    auto object = evaluate(expr->object.get());
    if (object.type() != typeid(MiniInstance*)) {
        throw RuntimeError(expr->name, "Only instances have fields.");
    }

    auto value = evaluate(expr->value.get());
    std::any_cast<MiniInstance*>(object)->set(expr->name, value);
    return value;
}

void Interpreter::execute_block(const std::vector<std::unique_ptr<Stmt>> &stmts, Enviroment env) {
    auto previous = std::move(this->enviroment);
    // TODO: catch exceptions
    //try {
        this->enviroment = std::make_shared<Enviroment>(std::move(env));
        FIXME<void()> f([&]() {
           this->enviroment = std::move(previous);
       });
        for (auto& stmt : stmts) {
            execute(stmt.get());
        }



    //}

}

void Interpreter::resolve(Expr *expr, int depth) {
    locals[expr] = depth;
}

std::any Interpreter::evaluate(Expr *expr) {
    return expr->accept(this);
}

void Interpreter::execute(Stmt *stmt) {
    stmt->accept(this);
}

void Interpreter::check_number_operand(Token op, const std::any &operand) {
    if (operand.type() == typeid(double)) return;
    throw RuntimeError(op, "Operand must be a number.");
}

void Interpreter::check_number_operands(Token op, const std::any &left, const std::any &right) {
    if (left.type() == typeid(double) && right.type() == typeid(double)) return;
    throw RuntimeError(op, "Operands must be numbers.");
}

bool Interpreter::isTruthy(const std::any &object) {
    if (object.type() == typeid(nullptr)) return false;
    if (object.type() == typeid(bool)) return std::any_cast<bool>(object);
    return true;
}

bool Interpreter::isEqual(const std::any &left, const std::any &right) {
    if (left.type() == typeid(nullptr) && right.type() == typeid(nullptr)) {
        return true;
    }
    if (left.type() == typeid(double) && right.type() == typeid(double)) {
        return std::any_cast<double>(left) == std::any_cast<double>(right);
    }
    if (left.type() == typeid(std::string) && right.type() == typeid(std::string)) {
        return std::any_cast<std::string>(left) == std::any_cast<std::string>(right);
    }
    if (left.type() == typeid(bool) && right.type() == typeid(bool)) {
        return std::any_cast<bool>(left) == std::any_cast<bool>(right);
    }
    return false;
}

std::string Interpreter::stringify(const std::any &object) {
    if (object.type() == typeid(nullptr)) return "nil";
    if (object.type() == typeid(double)) {
        return std::to_string(std::any_cast<double>(object));
    }
    if (object.type() == typeid(std::string)) {
        return std::any_cast<std::string>(object);
    }
    if (object.type() == typeid(bool)) {
        return std::any_cast<bool>(object) ? "true" : "false";
    }
}

