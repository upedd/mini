#include "Value.h"
#include "Object.h"
#include "base/overloaded.h"

std::string Value::to_string() const {
    return std::visit(
        overloaded {
            [](Nil) { return std::string("Nil"); },
            [](bite_int value) { return std::to_string(value); },
            [](bite_float value) { return std::to_string(value); },
            [](bool value) { return std::string(value ? "True" : "False"); },
            [](Object* object) { return object->to_string(); },
            [](std::string s) { return s; }
        },
        *this
    );
}
