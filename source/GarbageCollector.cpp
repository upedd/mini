//
// Created by MiłoszK on 08.07.2024.
//

#include "GarbageCollector.h"

void GarbageCollector::collect() {
    trace_references();
    sweep();
}

void GarbageCollector::mark(Object *object) {
    if (object == nullptr || object->is_marked) return;
    object->is_marked = true;
    grey_objects.push(object);
}

void GarbageCollector::mark(const Value &value) {
    if (auto object = value.as<Object*>()) {
        mark(*object);
    }
}

void GarbageCollector::add_object(Object *object) {
    objects.push_back(object);
}

void GarbageCollector::blacken_object(Object* object) {
    // Todo: refactor!
    if (auto *upvalue = dynamic_cast<Upvalue *>(object)) {
        mark(upvalue->closed);
    }
    if (auto *function = dynamic_cast<Function *>(object)) {
        for (auto &value: function->get_constants()) {
            mark(value);
        }
    }
    if (auto *closure = dynamic_cast<Closure *>(object)) {
        mark(closure->get_function());
        for (auto *upvalue: closure->upvalues) {
            mark(upvalue);
        }
    }

    if (auto *klass = dynamic_cast<Class*>(object)) {
        for (auto& [_, v] : klass->methods) {
            mark(v);
        }
    }

    if (auto *instance = dynamic_cast<Instance*>(object)) {
        mark(instance->klass);
        for (auto& [_, v] : instance->fields) {
            mark(v);
        }
    }

    if (auto *bound = dynamic_cast<BoundMethod*>(object)) {
        mark(bound->receiver);
        mark(bound->closure);
    }
}

void GarbageCollector::sweep() {
    std::erase_if(objects, [](Object* object) {
        if (object->is_marked) {
            object->is_marked = false;
            return false;
        }
        return true;
    });
}

void GarbageCollector::trace_references() {
    while (!grey_objects.empty()) {
        Object* grey = grey_objects.back(); grey_objects.pop();
        blacken_object(grey);
    }
}
