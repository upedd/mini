#include "Object.h"

int Function::add_constant(const Value &value) {
    constants.push_back(value);
    return constants.size() - 1;
}

Value Function::get_constant(int idx) {
    return constants[idx];
}

std::vector<Value> & Function::get_constants() {
    return constants;
}

void Function::add_allocated(Object *object) {
    allocated_objects.push_back(object);
}

const std::vector<Object *> Function::get_allocated() {
    return allocated_objects;
}

void Class::mark_references(GarbageCollector &gc) {
    for (auto &value: std::views::values(methods)) {
        gc.mark(value.value);
    }
    for (auto &value: std::views::values(fields)) {
        gc.mark(value.value);
    }
    for (auto *superclass: superclasses) {
        gc.mark(superclass);
    }
    gc.mark(constructor);
    gc.mark(class_object);
}
