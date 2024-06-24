#include "Value.h"

bool Value::is_bool() const {
    return type == Type::BOOL;
}

bool Value::is_nil() const {
    return type == Type::NIL;
}

bool Value::is_number() const {
    return type == Type::NUMBER;
}

bool Value::as_bool() const {
    return as.boolean;
}

double Value::as_number() const {
    return as.number;
}

Value Value::make_bool(bool value) {
    return Value {Type::BOOL, {.boolean = value}};
}

Value Value::make_nil() {
    return Value {Type::NIL, {.number = 0}};
}

Value Value::make_number(double value) {
    return Value {Type::NUMBER, {.number = value}};
}

std::string Value::to_string() const {
    if (is_bool()) {
        return as_bool() ? "True" : "False";
    } else if (is_number()) {
        return std::to_string(as_number());
    } else if (is_nil()) {
        return "nil";
    }
    return "UNKNOWN";
}
