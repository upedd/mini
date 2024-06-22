#include "Interpreter.h"

#include <chrono>
#include <iostream>
#include "Mini.h"
#include "MiniCallable.h"
#include "MiniFunction.h"
#include "TimeCall.h"

Interpreter::Interpreter() {
    // TODO memory leak?
    MiniCallable* ptr = new TimeCall();
    globals.define("time", ptr);
    enviroment = std::make_unique<Enviroment>(globals);
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

std::any Interpreter::visitVariableExpr(Expr::Variable *expr) {
    return enviroment->get(expr->name);
}

std::any Interpreter::visitAssignExpr(Expr::Assign *expr) {
    auto value = evaluate(expr->value.get());
    enviroment->assign(expr->name, value);
    return value;
}

std::any Interpreter::visitBlockStmt(Stmt::Block *stmt) {
    execute_block(stmt->statements, Enviroment(enviroment.get()));
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
    MiniCallable* function = new MiniFunction(stmt);
    enviroment->define(stmt->name.lexeme, function);
    return nullptr;
}

void Interpreter::execute_block(const std::vector<std::unique_ptr<Stmt>> &stmts, Enviroment env) {
    std::unique_ptr<Enviroment> previous = std::move(this->enviroment);
    // TODO: catch exceptions
    //try {
        this->enviroment = std::make_unique<Enviroment>(env);

        for (auto& stmt : stmts) {
            execute(stmt.get());
        }
    this->enviroment = std::move(previous);
    //}

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

