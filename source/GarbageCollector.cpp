#include "GarbageCollector.h"

#include "Object.h"
#include "Value.h"

void GarbageCollector::collect() {
    GC_LOG("== GC COLLECT START ===");
    trace_references();
    GC_LOG("Sweeping...");
    sweep();
    GC_LOG("== GC COLLECT END ===");
}

void GarbageCollector::mark(Object* object) {
    if (object == nullptr || object->is_marked) {
        return;
    }
    GC_LOG(std::format("Marked {}", object->to_string()));
    object->is_marked = true;
    grey_objects.push(object);
}

void GarbageCollector::mark(const Value& value) {
    if (auto object = value.as<Object*>()) {
        mark(*object);
    }
}

void GarbageCollector::add_object(Object* object) {
    GC_LOG(std::format("Started tracking {} size: {} bytes", object->to_string(), object->get_size()));
    memory_used += object->get_size();
    objects.push_back(object);
}


void GarbageCollector::sweep() {
    std::erase_if(
        objects,
        [this](Object* object) {
            if (object->is_marked) {
                object->is_marked = false;
                return false;
            }
            GC_LOG(std::format("Deleting {} size: {} bytes", object->to_string(), object->get_size()));
            memory_used -= object->get_size();
            delete object;
            return true;
        }
    );
}

void GarbageCollector::trace_references() {
    while (!grey_objects.empty()) {
        Object* grey = grey_objects.front();
        grey_objects.pop();
        GC_LOG(std::format("Tracing references for {}", grey->to_string()));
        grey->mark_references(*this);
    }
}
