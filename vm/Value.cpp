#include "Value.h"

ObjectString* ObjectString::allocate(const std::string& string) {
    auto* object = new ObjectString {Type::STRING, string};
    return object;
}

bool Value::is_bool() const {
    return type == Type::BOOL;
}

bool Value::is_nil() const {
    return type == Type::NIL;
}

bool Value::is_number() const {
    return type == Type::NUMBER;
}

bool Value::is_object() const {
    return type == Type::OBJECT;
}

bool Value::is_string() const {
    return is_object_type(Object::Type::STRING);
}

bool Value::as_bool() const {
    return as.boolean;
}

double Value::as_number() const {
    return as.number;
}

Object* Value::as_object() const {
    return as.object;
}

ObjectString* Value::as_string() const {
    return static_cast<ObjectString*>(as_object()); // NOLINT(*-pro-type-static-cast-downcast)
}

char* Value::as_cstring() const {
    return as_string()->string.data();
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

Value Value::make_object(Object *object) {
    return Value {Type::OBJECT, {.object =  object}};
}

std::string Value::to_string() const {
    if (is_bool()) {
        return as_bool() ? "True" : "False";
    }
    if (is_number()) {
        return std::to_string(as_number());
    }
    if (is_nil()) {
        return "nil";
    }
    if (is_object()) {
        switch (as_object()->type) {
            case Object::Type::STRING:
                return as_cstring();
        }
    }
    return "UNKNOWN";
}

bool Value::is_object_type(Object::Type type) const {
    return is_object() && as_object()->type == type;
}
